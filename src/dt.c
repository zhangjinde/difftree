/*****
 *
 * Description: Difftree Functions
 * 
 * Copyright (c) 2010-2015, Ron Dilley
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ****/

/****
 *
 * includes
 *
 ****/

#include <stdio.h>
#include <stdlib.h>

#include "dt.h"

/****
 *
 * local variables
 *
 ****/

/****
 *
 * global variables
 *
 ****/

PUBLIC int quit = FALSE;
PUBLIC int reload = FALSE;
PUBLIC Config_t *config = NULL;
PUBLIC int baseDirLen;
PUBLIC int compDirLen;
PUBLIC char *baseDir;
PUBLIC char *compDir;

/* hashes */
struct hash_s *baseDirHash = NULL;
struct hash_s *compDirHash = NULL;

/****
 *
 * external variables
 *
 ****/

extern int errno;
extern char **environ;

/****
 *
 * main function
 *
 ****/

int main(int argc, char *argv[]) {
  PRIVATE int pid = 0;
  PRIVATE int c = 0, i = 0, fds = 0, status = 0;
  int digit_optind = 0;
  PRIVATE struct passwd *pwd_ent;
  PRIVATE struct group *grp_ent;
  PRIVATE char **ptr;
  char *tmp_ptr = NULL;
  char *pid_file = NULL;
  char *user = NULL;
  char *group = NULL;
#ifdef LINUX
  struct rlimit rlim;

  getrlimit( RLIMIT_CORE, &rlim );
#ifdef DEBUG
  rlim.rlim_cur = rlim.rlim_max;
  printf( "DEBUG - RLIMIT_CORE: %ld\n", rlim.rlim_cur );
#else
  rlim.rlim_cur = 0; 
#endif
  setrlimit( RLIMIT_CORE, &rlim );
#endif

  /* setup config */
  config = ( Config_t * )XMALLOC( sizeof( Config_t ) );
  XMEMSET( config, 0, sizeof( Config_t ) );

  while (1) {
    int this_option_optind = optind ? optind : 1;
#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    static struct option long_options[] = {
      {"debug", required_argument, 0, 'd' },
      {"exdir"}, required_argument, 0, 'e' },
      {"exfile"}, required_argument, 0, 'E' },
      {"help", no_argument, 0, 'h' },
      {"logdir", required_argument, 0, 'l' },
      {"md5", no_argument, 0, 'm' },
      {"quick", no_argument, 0, 'q' },
      {"sha256", no_argument, 0, 's' },
      {"version", no_argument, 0, 'v' },
      {"write", required_argument, 0, 'w' },
      {0, no_argument, 0, 0}
    };
    c = getopt_long(argc, argv, "d:e:E:hl:mqsvw:", long_options, &option_index);
#else
    c = getopt( argc, argv, "d:e:E:hl:mqsvw:" );
#endif

    if (c EQ -1)
      break;

    switch (c) {

    case 'v':
      /* show the version */
      print_version();
      return( EXIT_SUCCESS );

    case 'd':
      /* show debig info */
      config->debug = atoi( optarg );
      config->mode = MODE_INTERACTIVE;
      break;

    case 'e':
      /* exclude a specific directory from the diff */
      if ( ( config->exclusions = (char **)XMALLOC( sizeof( char * ) * 2 ) ) EQ NULL ) {
        /* XXX problem */
      }
      if ( ( config->exclusions[0] = XMALLOC( MAXPATHLEN + 1 ) ) EQ NULL ) {
        /* XXX problem */
      }
      if ( optarg[0] != '/' ) {
        config->exclusions[0][0] = '/';
        XSTRNCPY( config->exclusions[0]+1, optarg, MAXPATHLEN - 1 );
      } else
        XSTRNCPY( config->exclusions[0], optarg, MAXPATHLEN );
      config->exclusions[1] = 0;
      break;
        
    case 'E':
      /* exclude a list of directories in the specific file */
      //if ( loadExclusions( optarg ) != TRUE )
      //  return( EXIT_FAILURE );
      //break;
      fprintf( stderr, "ERR - Feature not currently supported\n" );
      print_help();
      return( EXIT_SUCCESS );
      
    case 'h':
      /* show help info */
      print_help();
      return( EXIT_SUCCESS );

    case 'l':
      /* define the dir to store logs in */
      if ( ( config->log_dir = ( char * )XMALLOC( MAXPATHLEN + 1 ) ) EQ NULL ) {
        /* XXX problem */
      }
      XMEMSET( config->log_dir, 0, MAXPATHLEN + 1 );
      XSTRNCPY( config->log_dir, optarg, MAXPATHLEN );
      break;

    case 'w':
      /* define the dir to store logs in */
      if ( ( config->outfile = ( char * )XMALLOC( MAXPATHLEN + 1 ) ) EQ NULL ) {
        /* XXX problem */
      }
      XMEMSET( config->outfile, 0, MAXPATHLEN + 1 );
      XSTRNCPY( config->outfile, optarg, MAXPATHLEN );
      break;

    case 'm':
      /* md5 hash files */
      config->hash = TRUE;
      config->md5_hash = TRUE;
      config->digest_size = 16;
      config->sha256_hash = FALSE;
      break;

    case 'q':
      /* do quick checks only */
      config->quick = TRUE;
      break;
      
    case 's':
      /* sha256 hash files */
      config->hash = TRUE;
      config->sha256_hash = TRUE;
      config->digest_size = 32;
      config->md5_hash = FALSE;
      break;

    default:
      fprintf( stderr, "Unknown option code [0%o]\n", c);
    }
  }

  /* set default options */
  if ( config->log_dir EQ NULL ) {
    if ( ( config->log_dir = ( char * )XMALLOC( strlen( LOGDIR ) + 1 ) ) EQ NULL ) {
      /* XXX problem */
    }
    XSTRNCPY( config->log_dir, LOGDIR, strlen( LOGDIR ) );   
  }

  /* turn off quick mode if hash mode is enabled */
  if ( config->hash )
    config->quick = FALSE;

  /* enable syslog */
#ifdef HAVE_OPENLOG
  openlog( PROGNAME, LOG_CONS & LOG_PID, LOG_LOCAL0 );
#endif

  /* check dirs and files for danger */

  if ( time( &config->current_time ) EQ -1 ) {
    fprintf( stderr, "ERR - Unable to get current time\n" );
#ifdef HAVE_CLOSELOG
    /* cleanup syslog */
    closelog();
#endif
    /* cleanup buffers */
    cleanup();
    return EXIT_FAILURE;
  }

  /* initialize program wide config options */
  config->hostname = (char *)XMALLOC( MAXHOSTNAMELEN+1 );

  /* get processor hostname */
  if ( gethostname( config->hostname, MAXHOSTNAMELEN ) != 0 ) {
    fprintf( stderr, "Unable to get hostname\n" );
    strncpy( config->hostname, "unknown", MAXHOSTNAMELEN );
  }

  /* setup gracefull shutdown */
  signal( SIGINT, sigint_handler );
  signal( SIGTERM, sigterm_handler );
  signal( SIGFPE, sigfpe_handler );
  signal( SIGILL, sigill_handler );
  signal( SIGSEGV, sigsegv_handler );
#ifndef MINGW
  signal( SIGHUP, sighup_handler );
  signal( SIGBUS, sigbus_handler );
#endif  

  /****
   *
   * lets get this party started
   *
   ****/

  show_info();
  if ( ( baseDir = (char *)XMALLOC( PATH_MAX ) ) EQ NULL ) {
    fprintf( stderr, "ERR - Unable to allocate memory for baseDir string\n" );
    cleanup();
    return( EXIT_FAILURE );
  }
  if ( ( compDir = (char *)XMALLOC( PATH_MAX ) ) EQ NULL ) {
    fprintf( stderr, "ERR - Unable to allocate memory for compDir string\n" );
    cleanup();
    return( EXIT_FAILURE );
  }

  compDirHash = initHash( 52 );

  while (optind < argc ) {
    if ( ( compDirLen = strlen( argv[optind] ) ) >= PATH_MAX ) {
      fprintf( stderr, "ERR - Argument too long\n" );
      if ( baseDirHash != NULL )
	freeHash( baseDirHash );
      freeHash( compDirHash );
      cleanup();
      return( EXIT_FAILURE );
    } else {
      strncpy( compDir, argv[optind++], PATH_MAX-1 );
      /* process directory tree */
      if ( processDir( compDir ) EQ FAILED ) {
	if ( baseDirHash != NULL  )
	  freeHash( baseDirHash );
        freeHash( compDirHash );
	cleanup();
	return( EXIT_FAILURE );
      }

      if ( baseDirHash != NULL ) {
	/* compare the old tree to the new tree to find missing files */
	if ( traverseHash( baseDirHash, findMissingFiles ) != TRUE ) {
	  freeHash( baseDirHash );
	  freeHash( compDirHash );
	  cleanup();
	  return( EXIT_FAILURE );
	}
      }

      /* Prep for next dir to compare */
      if ( baseDirHash != NULL )
	freeHash( baseDirHash );
      baseDirHash = compDirHash;
      compDirHash = initHash( getHashSize( baseDirHash ) );
      baseDirLen = compDirLen;
      strncpy( baseDir, compDir, compDirLen );
    }
  }

  if ( baseDirHash != NULL )
    freeHash( baseDirHash );
  if ( compDirHash != NULL )
    freeHash( compDirHash );

  /****
   *
   * we are done
   *
   ****/

  /* cleanup syslog */
  closelog();

  cleanup();

  return( EXIT_SUCCESS );
}

/****
 *
 * display prog info
 *
 ****/

void show_info( void ) {
  fprintf( stderr, "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
  fprintf( stderr, "By: Ron Dilley\n" );
  fprintf( stderr, "\n" );
  fprintf( stderr, "%s comes with ABSOLUTELY NO WARRANTY.\n", PROGNAME );
  fprintf( stderr, "This is free software, and you are welcome\n" );
  fprintf( stderr, "to redistribute it under certain conditions;\n" );
  fprintf( stderr, "See the GNU General Public License for details.\n" );
  fprintf( stderr, "\n" );
}

/*****
 *
 * display version info
 *
 *****/

PRIVATE void print_version( void ) {
  printf( "%s v%s [%s - %s]\n", PROGNAME, VERSION, __DATE__, __TIME__ );
}

/*****
 *
 * print help info
 *
 *****/

PRIVATE void print_help( void ) {
  print_version();

  fprintf( stderr, "\n" );
  fprintf( stderr, "syntax: %s [options] {dir}|{file} [{dir} ...]\n", PACKAGE );

#ifdef HAVE_GETOPT_LONG
  fprintf( stderr, " -d|--debug (0-9)     enable debugging info\n" );
  fprintf( stderr, " -e|--exdir {dir}     exclude {dir}\n");
  fprintf( stderr, " -E|--exfile {file}   exclude directories listed in {file}\n");
  fprintf( stderr, " -h|--help            this info\n" );
  fprintf( stderr, " -l|--logdir {dir}    directory to create logs in (default: %s)\n", LOGDIR );
  fprintf( stderr, " -m|--md5             MD5 hash files and compare (disables -q|--quick and -s|--sha256 modes)\n" );
  fprintf( stderr, " -q|--quick           do quick comparisons only\n" );
  fprintf( stderr, " -s|--sha256          SHA256 hash files and compare (disables -q|--quick and -m|--md5 modes)\n" );
  fprintf( stderr, " -v|--version         display version information\n" );
  fprintf( stderr, " -w|--write {file}    write directory tree to file\n" );
#else
  fprintf( stderr, " -d {lvl}   enable debugging info\n" );
  fprintf( stderr, " -e {dir}   exclude {dir}\n");
  fprintf( stderr, " -E {file}  exclude directories listed in {file}\n");
  fprintf( stderr, " -h         this info\n" );
  fprintf( stderr, " -l {dir}   directory to create logs in (default: %s)\n", LOGDIR );
  fprintf( stderr, " -m         MD5 hash files and compare (disables -q and -s modes)\n" );
  fprintf( stderr, " -q         do quick comparisons only\n" );
  fprintf( stderr, " -s         SHA256 hash files and compare (disables -q and -m modes)\n" );
  fprintf( stderr, " -v         display version information\n" );
  fprintf( stderr, " -w {file}  write directory tree to file\n" );
#endif

  fprintf( stderr, "\n" );
}

/****
 *
 * cleanup
 *
 ****/

PRIVATE void cleanup( void ) {
  if ( baseDir != NULL )
    XFREE( baseDir );
  if ( compDir != NULL )
    XFREE( compDir );
  XFREE( config->hostname );
  if ( config->log_dir != NULL )
    XFREE( config->log_dir );
  if ( config->home_dir != NULL )
    XFREE( config->home_dir );
  if ( config->outfile != NULL )
    XFREE( config->outfile );
  XFREE( config );
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
}

/****
 *
 * SIGINT handler
 *
 ****/

void sigint_handler( int signo ) {
  signal( signo, SIG_IGN );

  /* do a calm shutdown as time and pcap_loop permit */
  quit = TRUE;
  signal( signo, sigint_handler );
}

/****
 *
 * SIGTERM handler
 *
 ****/

void sigterm_handler( int signo ) {
  signal( signo, SIG_IGN );

  /* do a calm shutdown as time and pcap_loop permit */
  quit = TRUE;
  signal( signo, sigterm_handler );
}

/****
 *
 * SIGHUP handler
 *
 ****/

#ifndef MINGW
void sighup_handler( int signo ) {
  signal( signo, SIG_IGN );

  /* time to rotate logs and check the config */
  reload = TRUE;
  signal( SIGHUP, sighup_handler );
}
#endif

/****
 *
 * SIGSEGV handler
 *
 ****/

void sigsegv_handler( int signo ) {
  signal( signo, SIG_IGN );

  fprintf( stderr, "ERR - Caught a sig%d, shutting down fast\n", signo );

  cleanup();
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  /* core out */
  abort();
}

/****
 *
 * SIGBUS handler
 *
 ****/

void sigbus_handler( int signo ) {
  signal( signo, SIG_IGN );

  fprintf( stderr, "ERR - Caught a sig%d, shutting down fast\n", signo );

  cleanup();
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  /* core out */
  abort();
}

/****
 *
 * SIGILL handler
 *
 ****/

void sigill_handler ( int signo ) {
  signal( signo, SIG_IGN );

  fprintf( stderr, "ERR - Caught a sig%d, shutting down fast\n", signo );

  cleanup();
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  /* core out */
  abort();
}

/****
 *
 * SIGFPE handler
 *
 ****/

void sigfpe_handler( int signo ) {
  signal( signo, SIG_IGN );

  fprintf( stderr, "ERR - Caught a sig%d, shutting down fast\n", signo );

  cleanup();
#ifdef MEM_DEBUG
  XFREE_ALL();
#endif
  /* core out */
  abort();
}
