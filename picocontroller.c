/* Copyright (C)
* 2020 - John Melton, G0ORX/N6LYT
  *
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <termios.h>

#include "band.h"
#include "channel.h"
#include "discovered.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "toolbar.h"
#include "radio.h"
#include "toolbar.h"
#include "main.h"
#include "property.h"
#include "vfo.h"
#include "wdsp.h"
#include "new_menu.h"
#include "encoder_menu.h"
#include "diversity_menu.h"
#include "actions.h"
#include "picocontroller.h"
#include "i2c.h"
#include "ext.h"
#include "sliders.h"
#include "new_protocol.h"
#include "zoompan.h"
#include "switch_menu.h"

char *picopropertiesfile = "picocontroller.props" ;

static GThread *picocontroller_thread_id;

PICOCONTROLLER picocontroller ={
    "/dev/ttyS0",
    "B115200"
};

PICOENCODER picoencoders[MAX_ENCODERS] = {
    // Encoder 2
    {TRUE, 2, ENCODER_RF_GAIN,
     TRUE, 6, ENCODER_AF_GAIN,
     TRUE, 11, MENU_BAND},

    // Encoder 3
    {TRUE, 3, ENCODER_ATTENUATION,
     TRUE, 7, ENCODER_AGC_GAIN,
     TRUE, 12, MENU_MODE},

    // Encoder 4
    {TRUE, 4, ENCODER_IF_WIDTH,
     TRUE, 8, ENCODER_IF_SHIFT,
     TRUE, 13, MENU_FILTER},

    // Encoder 5
    {TRUE, 5, ENCODER_XIT,
     TRUE, 9, ENCODER_RIT,
     TRUE, 14, MENU_FREQUENCY},

    // VFO Encoder
    {TRUE, 1, ENCODER_VFO,
     FALSE, 0, ENCODER_NO_ACTION,
     FALSE, 0, NO_ACTION},
};

PICOSWITCH picoswitches[MAX_PICOSWITCHES] = {
    {FALSE, 15, MICPTT},     // External PTT
    {FALSE, 16, MICPTT},     // Microphone PTT
    {FALSE, 17, BAND_MINUS}, // Microphone Down Button
    {FALSE, 18, BAND_PLUS},  // Microphone Up Button
    {FALSE, 19, MUTE},       // Microphone FST Button
    {FALSE, 20, XIT},
    {FALSE, 21, BAND_MINUS},
    {FALSE, 22, BAND_PLUS},
    {FALSE, 23, MODE_MINUS},
    {FALSE, 24, MODE_PLUS},
    {FALSE, 25, A_TO_B},
    {FALSE, 26, B_TO_A},
    {FALSE, 27, LOCK},
    {FALSE, 28, CTUN},
    {FALSE, 29, MOX},
    {FALSE, 30, TUNE},
    {FALSE, 31, PS},
    {FALSE, 32, SPLIT},
    {FALSE, 33, NR},
    {FALSE, 34, NB},
    {FALSE, 35, SNB}};

int serial_port;
gboolean stop_serial = FALSE;
struct termios tty;

static gpointer picocontroller_thread(gpointer arg) {

    int address = 0;
    int value = 0;
    
    char read_buf[256];
    char str[100];
        
    while (!stop_serial) {
        int n = read(serial_port, &read_buf, sizeof(read_buf));
        
        if (n > 0) {
            for (int i=0; i < n; i++) {
                address = read_buf[i] >> 1;
                value = read_buf[i] & 1;
                
                sprintf(str, "PicoController: Control %d sent command %d\n", address, value);
                printf(str);
                if (address < 10) {
                    ENCODER_ACTION *e = g_new(ENCODER_ACTION, 1);
                    
                    e->action = pico_find_encoder(address);
                    e->mode=RELATIVE;
                    e->val= value == 1 ? 1 : -1;
                    
                    g_idle_add(encoder_action, e);
                }
                else {
                    SWITCH_ACTION *a=g_new(SWITCH_ACTION,1);
                    
                    a->action = pico_find_switch(address);
                    a->state = value == 1 ? 0 : 1;
                    
                    g_idle_add(switch_action,a);
                }
            }
        }
    }
    
    return NULL;
}

gint  pico_find_switch(int address) {
    
    if (address >= 15) {
        for (int x = 0; x < sizeof(picoswitches); x++) {
            if ( picoswitches[x].switch_address == address) {
                return picoswitches[x].switch_function;
            }
        }
    } 
    else if (address < 15) {
        for (int x = 0; x < sizeof(picoencoders); x++) {
            if ( picoencoders[x].switch_address == address) {
                return picoencoders[x].switch_function;
            }
        } 
    }
    
    return NO_ACTION;
}

gint  pico_find_encoder(int address) {
    
    if (address < 6) {
        for (int x = 0; x < sizeof(picoencoders); x++) {
            if ( picoencoders[x].bottom_encoder_address == address) {
                return picoencoders[x].bottom_encoder_function;
            }
        }
    }
    else if (address < 10) {
        for (int x = 0; x < sizeof(picoencoders); x++) {
            if ( picoencoders[x].top_encoder_address == address) {
                return picoencoders[x].top_encoder_function;
            }
        }
    }
        
    return ENCODER_NO_ACTION;
}

void pico_configure(GtkWidget *notebook, GtkWidget *grid) {
    
        int col = 0;
        int row = 0;
        GtkWidget *widget;
    
    widget=gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "Serial Device:");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,2,1);
    col+=2;

    widget=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget), picocontroller.controller_device);
    //widget.activate.connect( () => { picocontroller.controller_device = widget.get_text(); } );
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    col = 0;
    row++;

    widget=gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;

    widget=gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL(widget), "Serial Speed:");
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,2,1);
    col+=2;

    widget=gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widget), picocontroller.baudrate);
    gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
    col++;
   
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),grid,gtk_label_new("PIcoHPSDR Controller"));
}

void pico_configure_encoders() {
}

void pico_configure_switches(){
//         row=row+5;
//     col=0;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[0].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(0));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[1].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(1));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[2].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(2));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[3].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(3));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[4].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(4));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[5].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(5));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[6].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(6));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
// 
//     row=original_row;
//     col=8;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[7].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(7));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     row++;
//     col=7;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[8].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(8));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[9].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(9));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     row++;
//     col=7;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[10].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(10));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[11].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(11));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     row++;
//     col=7;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[12].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(12));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[13].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(13));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     row++;
//     col=7;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[14].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(14));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
//     col++;
//     widget=gtk_button_new_with_label(sw_string[temp_switches[15].switch_function]);
//     g_signal_connect(widget,"button_press_event",G_CALLBACK(switch_cb),GINT_TO_POINTER(15));
//     gtk_grid_attach(GTK_GRID(grid),widget,col,row,1,1);
// 
//     gtk_container_add(GTK_CONTAINER(content),grid);
}
    
void pico_set_defaults(int ctrlr) {

  g_print("%s: %d\n",__FUNCTION__,ctrlr);
  
}
    
void pico_restore_actions() {
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char name[80];
    char *value;
    for (int i = 0; i < MAX_ENCODERS; i++)
    {
        sprintf(name, "picoencoders[%d].bottom_encoder_function", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].bottom_encoder_function = atoi(value);

        sprintf(name, "picoencoders[%d].top_encoder_function", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].top_encoder_function = atoi(value);

        sprintf(name, "picoencoders[%d].switch_function", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].switch_function = atoi(value);
    }

    for (int i = 0; i < MAX_PICOSWITCHES; i++)
    {
        sprintf(name, "picoswitches[%d].switch_function", i);
        value = getProperty(name);
        if (value)
            picoswitches[i].switch_function = atoi(value);
    }
}

void pico_restore_state() {
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char *value;
    char name[80];

    loadProperties(picopropertiesfile);

    controller = NO_CONTROLLER;
    value = getProperty("controller");
    if (value)
        controller = atoi(value);

    // Restore general configuration
    sprintf(name, "picocontroller.controller_device");
    value = getProperty(name);
    if (value)
        picocontroller.controller_device = value;
    
    sprintf(name, "picocontroller.baudrate");
    value = getProperty(name);
    if (value)
        picocontroller.baudrate = value;
    
    
    for (int i = 0; i < MAX_PICOENCODERS; i++)
    {
        sprintf(name, "picoencoders[%d].bottom_encoder_enabled", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].bottom_encoder_enabled = atoi(value);

        sprintf(name, "picoencoders[%d].bottom_encoder_address", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].bottom_encoder_address = atoi(value);

        value = getProperty(name);
        sprintf(name, "picoencoders[%d].top_encoder_enabled", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].top_encoder_enabled = atoi(value);

        sprintf(name, "picoencoders[%d].top_encoder_address", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].top_encoder_address = atoi(value);

        sprintf(name, "picoencoders[%d].switch_enabled", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].switch_enabled = atoi(value);

        sprintf(name, "picoencoders[%d].switch_address", i);
        value = getProperty(name);
        if (value)
            picoencoders[i].switch_address = atoi(value);
    }

    for (int i = 0; i < MAX_PICOSWITCHES; i++)
    {
        sprintf(name, "picoswitches[%d].switch_enabled", i);
        value = getProperty(name);
        if (value)
            picoswitches[i].switch_enabled = atoi(value);

        sprintf(name, "picoswitches[%d].switch_address", i);
        value = getProperty(name);
        if (value)
            picoswitches[i].switch_address = atoi(value);
    }
}

void pico_save_actions() {
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char value[80];
    char name[80];
    for (int i = 0; i < MAX_ENCODERS; i++)
    {
        sprintf(name, "picoencoders[%d].bottom_encoder_function", i);
        sprintf(value, "%d", picoencoders[i].bottom_encoder_function);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].top_encoder_function", i);
        sprintf(value, "%d", picoencoders[i].top_encoder_function);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].switch_function", i);
        sprintf(value, "%d", picoencoders[i].switch_function);
        setProperty(name, value);
    }

    for (int i = 0; i < MAX_PICOSWITCHES; i++)
    {
        sprintf(name, "picoswitches[%d].switch_function", i);
        sprintf(value, "%d", picoswitches[i].switch_function);
        setProperty(name, value);
    }
}

void pico_save_state() {
    
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char value[80];
    char name[80];

    clearProperties();
    sprintf(value, "%d", controller);
    setProperty("controller", value);

    // Save general configuration
    sprintf(name, "picocontroller.controller_device");
    sprintf(value, picocontroller.controller_device);
    setProperty(name, value);
    
    sprintf(name, "picocontroller.baudrate");
    sprintf(value, picocontroller.baudrate);
    setProperty(name, value);
    
    for (int i = 0; i < MAX_PICOENCODERS; i++)
    {
        sprintf(name, "picoencoders[%d].bottom_encoder_enabled", i);
        sprintf(value, "%d", picoencoders[i].bottom_encoder_enabled);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].bottom_encoder_address", i);
        sprintf(value, "%d", picoencoders[i].bottom_encoder_address);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].top_encoder_enabled", i);
        sprintf(value, "%d", picoencoders[i].top_encoder_enabled);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].top_encoder_address", i);
        sprintf(value, "%d", picoencoders[i].top_encoder_address);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].switch_enabled", i);
        sprintf(value, "%d", picoencoders[i].switch_enabled);
        setProperty(name, value);

        sprintf(name, "picoencoders[%d].switch_address", i);
        sprintf(value, "%d", picoencoders[i].switch_address);
        setProperty(name, value);
    }

    for (int i = 0; i < MAX_PICOSWITCHES; i++)
    {
        sprintf(name, "picoswitches[%d].switch_enabled", i);
        sprintf(value, "%d", picoswitches[i].switch_enabled);
        setProperty(name, value);

        sprintf(name, "picoswitches[%d].switch_address", i);
        sprintf(value, "%d", picoswitches[i].switch_address);
        setProperty(name, value);
    }

    saveProperties(picopropertiesfile);
}

int pico_init() {
    int ret = 0;
    
    if (controller != PICOHPSDR_CONTROLLER)
        return ret;
  
    printf("Opening serial port for picohpsdrcontroller\n");
    serial_port = open(picocontroller.controller_device, O_RDWR);
        
    if (serial_port < 0) {
            printf("ERROR %i from open serial port: %s\n", errno, strerror(errno));
            exit(-1);
    }
    
    if ( tcgetattr(serial_port, &tty) != 0 ) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        exit(-1);
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
            
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;
 
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        exit(-1);
    } 
    
    picocontroller_thread_id = g_thread_new( "picohpsdrcontroller", picocontroller_thread, NULL);

    if(!picocontroller_thread_id ) {
        g_print("%s: g_thread_new failed on picohpsdrcontroller_thread\n",__FUNCTION__);
        exit( -1 );
    }
    else {
      g_print("%s: picohpsdrcontroller_thread: id=%p\n", __FUNCTION__, picocontroller_thread_id);
    }
    
    return ret;
}

void pico_close()
{
    stop_serial = TRUE;
    close(serial_port);
}
