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

char *picopropertiesfile = "picocontroller.props" ;

PICOCONTROLLER picocontroller ={
    "/dev/ttyS0",
    "115200"
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

    // Encoder 6
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

void pico_restore_actions()
{
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

void pico_restore_state()
{
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char *value;
    char name[80];

    loadProperties(picopropertiesfile);

    controller = NO_CONTROLLER;
    value = getProperty("controller");
    if (value)
        controller = atoi(value);

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

void pico_save_actions()
{
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

void pico_save_state()
{
    if (controller != PICOHPSDR_CONTROLLER)
        return;

    char value[80];
    char name[80];

    clearProperties();
    sprintf(value, "%d", controller);
    setProperty("controller", value);

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

int pico_init()
{
    int ret = 0;
    return ret;
}

void pico_close()
{
}
