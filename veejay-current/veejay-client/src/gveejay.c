/* gveejay - Linux VeeJay - GVeejay GTK+-2/Glade User Interface
 *           (C) 2002-2011 Niels Elburg <nwelburg@gmail.com> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <veejay/vevo.h>
#include <veejay/vjmem.h>
#include <veejay/vj-msg.h>
#include <veejay/libvevo.h>
#include <src/vj-api.h>

static int selected_skin = 0;
extern int mt_get_max_tracks();
static int load_midi = 0;
static int port_num	= DEFAULT_PORT_NUM;
static char hostname[255];
static int gveejay_theme = 0; // set to 1 to load with the default reloaded theme 
static int verbosity = 0;
static int col = 0;
static int row = 0;
static int n_tracks = 7;
static int launcher = 0;
static int preview = 0; // off
static int use_threads = 0;
static char midi_file[1024];
static int beta = 0;
static int auto_connect = 0;
static int geom_[2] = { -1 , -1};
static struct
{
	char *file;
} skins[] = {
 {	"gveejay.reloaded.glade" },
 {	"reloaded_classic.glade" },
 {	NULL }
};

extern void reloaded_launcher( char *h, int p );

static void usage(char *progname)
{
	printf( "Usage: %s <options>\n",progname);
	printf( "where options are:\n");
	printf( "-h\t\tVeejay host to connect to (defaults to localhost) \n");
	printf( "-p\t\tVeejay port to connect to (defaults to %d) \n", DEFAULT_PORT_NUM);
	printf( "-t\t\tLoad gveejay's classic GTK theme\n");
	printf( "-n\t\tDont use colored text\n");
	printf( "-v\t\tBe extra verbose (usefull for debugging)\n");
	printf( "-s\t\tSet bank resolution (row X columns)\n");
	printf( "-P\t\tStart with preview enabled (1=1/1,2=1/2,3=1/4,4=1/8)\n");
	printf( "-X\t\tSet number of tracks\n");
	printf( "-l\t\tChoose layout (0=large screen, 1=small screens)\n");
	printf( "-V\t\tShow version, data directory and exit.\n");
	printf( "-m <file>\tMIDI configuration file.\n");
	printf( "-g\t\t<X,Y>\tWindow position on screen.\n");
	printf( "-b\t\tEnable beta features.\n");
	printf( "-a\t\tAuto-connect to local running veejays.\n");
	printf( "-L\t\tLow-bandwith connection (disables image loading in samplebank)\n");

	printf( "\n\n");
	exit(-1);
}

static int set_option( const char *name, char *value )
{
	int err = 0;
	if( strcmp(name, "h") == 0 || strcmp(name, "hostname") == 0 )
	{
		strcpy( hostname, optarg );
		launcher ++;
	}
	else if( strcmp(name, "p") == 0 || strcmp(name ,"port") == 0 )
	{
		if(sscanf( optarg, "%d", &port_num ))
		launcher++;
	}
	else if (strcmp(name, "l" ) == 0 ) {
		selected_skin = atoi( optarg);
	}
	else if (strcmp(name, "n") == 0 )
	{
		veejay_set_colors(0);
	}
	else if (strcmp(name, "X") == 0 )
	{
		n_tracks = 1 + atoi(optarg); 
		if( n_tracks < 1 || n_tracks > mt_get_max_tracks() )
			n_tracks = 1;
	}
	else if( strcmp(name, "t") == 0 || strcmp(name, "no-theme") == 0)
	{
		gveejay_theme = 1;
	}
	else if( strcmp(name, "v") == 0 || strcmp(name, "verbose") == 0)
	{
		verbosity = 1;
	}
	else if (strcmp(name, "s") == 0 || strcmp(name, "size") == 0)
	{
		if(sscanf( (char*) optarg, "%dx%d",
			&row, &col ) != 2 )
		{
			fprintf(stderr, "--size parameter requires NxN argument");
			err++;
		}
	}
	else if (strcmp(name, "V") == 0 )
	{
		fprintf(stdout, "version: %s\n", PACKAGE_VERSION);
		fprintf(stdout, "data directory: %s\n", get_gveejay_dir());
		exit(0);
	}
	else if (strcmp( name, "m" ) == 0 ) {
		strcpy(midi_file, optarg);
		load_midi = 1;
	}
	else if (strcmp(name,"g") == 0 ) {
		if(sscanf( optarg, "%d,%d",&geom_[0],&geom_[1]) != 2 ) {
			fprintf(stderr, "invalid screen coordinates:%s\n",optarg);
		} else {
			fprintf(stdout, "Place window at %d,%d", geom_[0],geom_[1]);
			vj_gui_set_geom(geom_[0],geom_[1]);
		}

	}
	else if (strcmp(name, "P" ) == 0 || strcmp(name, "preview" ) == 0 )
	{
		preview = atoi(optarg);
		if(preview <= 0 ) {
			preview = 2;
		}
		if(preview > 4 )
		{
			fprintf(stderr, "--preview [0-4]\n");
			err++;
		}
	}
	else if (strcmp(name,"b") == 0 || strcmp(name, "beta" ) == 0 )
	{
		beta = 1;
	}
	else if (strcmp(name,"a") == 0 )
	{
		auto_connect = 1;
	}
	else if( strcmp(name,"L") == 0 )
	{
		set_disable_sample_image(1);
	}
	else
		err++;
	return err;
}

static volatile gulong g_trap_free_size = 0;
static struct timeval time_last_;

static char **cargv = NULL;

gboolean gveejay_idle(gpointer data)
{
	if(gveejay_running())
	{
		int sync = 0;
		if( is_alive(&sync) == FALSE )
		{
			return FALSE;
		} 
		if( sync )
		{
			if( gveejay_time_to_sync( get_ui_info() ) )
			{
				veejay_update_multitrack( get_ui_info() );
			}
		} 

		update_gveejay();

	}
	if( gveejay_restart() )
	{
		//@ reinvoke 
		if( execvp( cargv[0], cargv ) == -1 )
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to restart");
	}

	return TRUE;
}

static	void	clone_args( char *argv[], int argc )
{
	int i = 0;
	if( argc <= 0 )
		return;

	cargv = (char**) malloc(sizeof(char*) * (argc+1) );
	memset( cargv, 0, sizeof(char*) * (argc+1));
	for( i = 0; i < argc ; i ++ )
		cargv[i] = strdup( argv[i] );
}

/*
static void
activate (GtkApplication* app,
          gpointer        user_data)
{
  GtkWidget *window;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
  gtk_widget_show_all (window);
}

int main(int argc, char **argv)
{
	char option[2];
	int n;
	int err=0;

	if(!argc) usage(argv[0]);
	clone_args( argv, argc );

  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
*/

int main (int argc, char *argv[])
{
	char option[2];
	int n;
	int err=0;

	if(!argc) usage(argv[0]);
	clone_args( argv, argc );

#if !GLIB_CHECK_VERSION(2,36,0)
	g_type_init();
#endif

  gtk_init( &argc, &argv );
	// default host to connect to
	snprintf(hostname,sizeof(hostname), "127.0.0.1");

	while( ( n = getopt( argc, argv, "s:h:p:tabnvLHfX:P:Vl:T:m:g:")) != EOF )
	{
		sprintf(option, "%c", n );
		err += set_option( option, optarg);
		if(err) usage(argv[0]);
	}
	if( optind > argc )
		err ++;

	if( err ) usage(argv[0]);

	vj_mem_init();

	vevo_strict_init();

// FIXME OLDRC
	//~ find_user_themes(gveejay_theme);

	vj_gui_set_debug_level( verbosity , n_tracks,0,0);
	set_skin( selected_skin, gveejay_theme );

	default_bank_values( &col, &row );
//	gui_load_theme(); FIXME OLDRC

	register_signals();

	vj_gui_init( skins[selected_skin].file, launcher, hostname, port_num, use_threads, load_midi, midi_file,beta,auto_connect);
	vj_gui_style_setup();

	if( preview )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Starting with preview enabled");
		gveejay_preview(preview);
	}

restart_me:

	reloaded_show_launcher ();
	if( launcher )
	{
		reloaded_launcher( hostname, port_num );
	}

	memset( &time_last_, 0, sizeof(struct timeval));

	while(gveejay_running())
	{
		if(gveejay_idle(NULL)==FALSE)
			break;
		while( gtk_events_pending()  )
			gtk_main_iteration();
	}

	vj_event_list_free();

	if( gveejay_relaunch() ) {
		launcher = 1;
		reloaded_restart();
		goto restart_me;
	}

	veejay_msg(VEEJAY_MSG_INFO, "See you!");

	return 0;
}
