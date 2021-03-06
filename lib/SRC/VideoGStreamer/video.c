/*
 * Video capture module utilising the GStreamer pipeline for AR Toolkit
 * 
 * (c) Copyrights 2003-2013 Hartmut Seichter <http://www.technotecture.com>
 * 
 * licensed under the terms of the LGPL v2
 *
 */

/* include AR Toolkit*/ 
#include <AR/config.h>
#include <AR/ar.h>
#include <AR/video.h>
	 
/* include GLib for GStreamer */
#include <glib.h>
#include <glib/gprintf.h>

/* include GStreamer itself */
#include <gst/gst.h>

/* using memcpy */
#include <string.h>

#define GSTREAMER_1_0

#define GSTREAMER_LAUNCH_CFG "%s ! queue ! colorspace ! video/x-raw-rgb,bpp=24,depth=24,width=%d,height=%d ! identity name=artoolkit ! fakesink sync=true"
#define GSTREAMER_TEST_LAUNCH_CFG "videotestsrc"


struct _AR2VideoParamT {
	
	/* size of the image */
	int	width, height;

	/* the actual video buffer */
    ARUint8             *videoBuffer;

	/* GStreamer pipeline */
	GstElement *pipeline;
	
	/* GStreamer identity needed for probing */
	GstElement *probe;

	/* give external APIs a hint of progress */
	unsigned int frame;
	unsigned int frame_req;

};


static AR2VideoParamT *gVid = NULL;


GstPadProbeReturn 
	_artoolkit_data_callback(
GstPad    *pad,
GstPadProbeInfo *padInfo,
gpointer   u_data)
{

 	const GstCaps *caps;
	GstStructure *str;
	GstMapInfo mapInfo;
	
	gint width,height;
	gdouble rate;
	
	GstBuffer* buffer = gst_pad_probe_info_get_buffer(padInfo);

	AR2VideoParamT *vid = (AR2VideoParamT*)u_data;

	if (vid == NULL) return GST_PAD_PROBE_DROP;
	

	/* only do initially for the buffer */
	if (vid->videoBuffer == NULL && buffer) 
	{
		g_print("libARvideo error! Buffer not allocated\n");		
	}

	if (vid->videoBuffer)
	{
		vid->frame++;

#if defined(GSTREAMER_1_0)

		if (gst_buffer_map(buffer,&mapInfo,GST_MAP_READ)) {
			memcpy(vid->videoBuffer,mapInfo.data,mapInfo.size);
		}

#else
		memcpy(vid->videoBuffer, buffer->data, buffer->size);		
#endif
	}
	
	return GST_PAD_PROBE_OK;
}


static
void video_caps_notify(GObject* obj, GParamSpec* pspec, gpointer data) {

	const GstCaps *caps;
	GstStructure *str;
	
	gint width,height;
	gdouble rate;
	
	AR2VideoParamT *vid = (AR2VideoParamT*)data;

	caps = gst_pad_get_current_caps((GstPad*)obj);

	if (caps) {

		str=gst_caps_get_structure(caps,0);

		/* Get some data about the frame */
		gst_structure_get_int(str,"width",&width);
		gst_structure_get_int(str,"height",&height);
		gst_structure_get_double(str,"framerate",&rate);
		
		g_print("libARvideo: GStreamer negotiated %dx%d @%3.3fps\n", width, height,rate);

		vid->width = width;
		vid->height = height;

		g_print("libARvideo: allocating %d bytes\n",(vid->width * vid->height * AR_PIX_SIZE_DEFAULT));

		/* allocate the buffer */	
		arMalloc(vid->videoBuffer, ARUint8, (vid->width * vid->height * AR_PIX_SIZE_DEFAULT) );

	}
}


int
arVideoOpen( char *config ) {
   if( gVid != NULL ) {
        printf("Device has been opened!!\n");
        return -1;
    }
    gVid = ar2VideoOpen( config );
    if( gVid == NULL ) return -1;
}

int 
arVideoClose( void )
{
	return ar2VideoClose(gVid);
}

int
arVideoDispOption( void )
{
   return 0;
}

int
arVideoInqSize( int *x, int *y ) {
	
	ar2VideoInqSize(gVid,x,y);

	return 0;
}

ARUint8
*arVideoGetImage( void )
{
   return ar2VideoGetImage(gVid);  // address of your image data
}

int 
arVideoCapStart( void ) {

	ar2VideoCapStart(gVid);
	return 0;
}

int 
arVideoCapStop( void )
{
	ar2VideoCapStop(gVid);
	return 0;
}

int arVideoCapNext( void )
{
	return ar2VideoCapNext(gVid);;
}

/*---------------------------------------------------------------------------*/

AR2VideoParamT* 
ar2VideoOpen(char *config_in ) {

    int width = 640;
    int height = 480;


	AR2VideoParamT *vid = 0;
	GError *error = 0;
	int i;
	GstPad *pad, *peerpad;
	//GstXML *xml;
	GstStateChangeReturn _ret;
	int is_live;
    char *srcConfig = 0;
    char *fullConfig = 0;

	/* If no config string is supplied, we should use the environment variable, otherwise set a sane default */
	if (!config_in || !(config_in[0])) {
		/* None supplied, lets see if the user supplied one from the shell */
		char *envconf = getenv ("ARTOOLKIT_CONFIG");
		if (envconf && envconf[0]) {
            srcConfig = envconf;
			g_printf ("Using config string from environment [%s].\n", envconf);
		} else {
            srcConfig = NULL;

			g_printf ("Warning: no video config string supplied and ARTOOLKIT_CONFIG not set. Using default!.\n");

			/* setting up defaults - we fall back to the TV test signal simulator */
            srcConfig = GSTREAMER_TEST_LAUNCH_CFG;
				
		}

	} else {
        srcConfig = config_in;
    }

    /* build the config string for launcher */
    fullConfig = g_strdup_printf(GSTREAMER_LAUNCH_CFG,srcConfig,width,height);

    g_printf ("GStreamer launch [%s].\n", fullConfig);

	/* initialise GStreamer */
	gst_init(0,0);	
	
	/* init ART structure */
    arMalloc( vid, AR2VideoParamT, 1 );

	/* initialise buffer */
	vid->videoBuffer = NULL;
	
	/* report the current version and features */
	g_print ("libARvideo: %s\n", gst_version_string());

#if 0	
	xml = gst_xml_new();
	
	/* first check if config contains an xml file */
	if (gst_xml_parse_file(xml,config,NULL)) 
	{
		/* parse the pipe definition */
		
	} else 
	{
		vid->pipeline = gst_xml_get_element(xml,"pipeline");
	}
	
#endif

    vid->pipeline = gst_parse_launch (fullConfig, &error);


	if (!vid->pipeline) {
		g_print ("Parse error: %s\n", error->message);
		return 0;
	};

	/* get the video sink */
	vid->probe = gst_bin_get_by_name(GST_BIN(vid->pipeline), "artoolkit");

	if (!vid->probe) {
		g_print("Pipeline has no element named 'artoolkit'!\n");
		return 0;	
	};

	/* get the pad from the probe (the source pad seems to be more flexible) */	
	pad = gst_element_get_static_pad (vid->probe, "src");

	/* get the peerpad aka sink */
	peerpad = gst_pad_get_peer(pad);

	/* install the probe callback for capturing */	
	gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, _artoolkit_data_callback, vid, NULL);	

	g_signal_connect(pad, "notify::caps", G_CALLBACK(video_caps_notify), vid);

	/* Needed to fill the information for ARVidInfo */
	gst_element_set_state (vid->pipeline, GST_STATE_READY);

	/* wait until it's up and running or failed */
	if (gst_element_get_state (vid->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    	g_error ("libARvideo: failed to put GStreamer into READY state!\n");
    } else {

		is_live = (_ret == GST_STATE_CHANGE_NO_PREROLL) ? 1 : 0;			
    	g_print ("libARvideo: GStreamer pipeline is READY!\n");
    }

	/* Needed to fill the information for ARVidInfo */
	_ret = gst_element_set_state (vid->pipeline, GST_STATE_PAUSED);

	is_live = (_ret == GST_STATE_CHANGE_NO_PREROLL) ? 1 : 0;

	/* wait until it's up and running or failed */
	if (gst_element_get_state (vid->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {		
    	g_error ("libARvideo: failed to put GStreamer into PAUSED state!\n");
    } else {
    	g_print ("libARvideo: GStreamer pipeline is PAUSED!\n",is_live);
    }
	
	/* dismiss the pad */
	gst_object_unref (pad);

	/* dismiss the peer-pad */
	gst_object_unref (peerpad);

	/* now preroll for live sources */
	if (is_live) {

		g_print ("libARvdeo: need special prerolling for live sources\n"); 

		/* set playing state of the pipeline */
		gst_element_set_state (vid->pipeline, GST_STATE_PLAYING);
		
		/* wait until it's up and running or failed */
		if (gst_element_get_state (vid->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {
	    	g_error ("libARvideo: failed to put GStreamer into PLAYING state!\n");
	    } else {
	    	g_print ("libARvideo: GStreamer pipeline is PLAYING!\n");
	    }
		
		/* set playing state of the pipeline */
		gst_element_set_state (vid->pipeline, GST_STATE_PAUSED);
		
		/* wait until it's up and running or failed */
		if (gst_element_get_state (vid->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {
	    	g_error ("libARvideo: failed to put GStreamer into PAUSED state!\n");
	    } else {
	    	g_print ("libARvideo: GStreamer pipeline is PAUSED!\n");
	    }
	}
		
#if 0
	/* write the bin to stdout */
	gst_xml_write_file (GST_ELEMENT (vid->pipeline), stdout);
#endif
	
	/* return the video handle */
	return vid;
};


int 
ar2VideoClose(AR2VideoParamT *vid) {


	/* stop the pipeline */
	gst_element_set_state (vid->pipeline, GST_STATE_NULL);

	/* wait until it'stops or failed */
	if (gst_element_get_state (vid->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {
		g_error ("libARvideo: failed to put GStreamer into STOPPED state!\n");
	} else {
		g_print ("libARvideo: GStreamer pipeline is STOPPED!\n");
	}
	
	/* free the pipeline handle */
	gst_object_unref (GST_OBJECT (vid->pipeline));

	return 0;
}


ARUint8* 
ar2VideoGetImage(AR2VideoParamT *vid) {
	/* just return the bare video buffer */
	return vid->videoBuffer;
}

int 
ar2VideoCapStart(AR2VideoParamT *vid) 
{
	GstStateChangeReturn _ret;

	/* set playing state of the pipeline */
	_ret = gst_element_set_state (vid->pipeline, GST_STATE_PLAYING);

	if (_ret == GST_STATE_CHANGE_ASYNC) 
	{

		/* wait until it's up and running or failed */
		if (gst_element_get_state (vid->pipeline, 
				NULL, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_FAILURE) 
		{
    		g_error ("libARvideo: failed to put GStreamer into PLAYING state!\n");    	
    		return 0;
  
        } else {
			g_print ("libARvideo: GStreamer pipeline is PLAYING!\n");
		} 
	}
	return 1; 
}

int 
ar2VideoCapStop(AR2VideoParamT *vid) {
	/* stop pipeline */
	return gst_element_set_state (vid->pipeline, GST_STATE_PAUSED);
}

int 
ar2VideoCapNext(AR2VideoParamT *vid)
{
	if (vid && vid->frame != vid->frame_req) {
		vid->frame_req = vid->frame;
		return 0;
	}
	return -1;
}

int
ar2VideoInqSize(AR2VideoParamT *vid, int *x, int *y ) 
{
   if(vid && x && y) {

      *x = vid->width; // width of your static image
      *y = vid->height; // height of your static image
      g_print ("libARvideo: reported size: %dx%d\n", *x, *y);

   }
}

