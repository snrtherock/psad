/*
******************************************************************************
*
*  File: kmsgsd.c
*
*  Purpose: kmsgsd separates ipchains/iptables messages from all other
*           kernel messages.
*
*  Strategy: read messages from the /var/log/psadfifo named pipe and
*            print any firewall related dop/reject/deny messages to
*            the psad data file "/var/log/psad/fwdata".
*
*  Author: Michael B. Rash (mbr@cipherdyne.com)
*
*  Credits:  (see the CREDITS file)
*
*  Version: 1.0.0-pre4
*
*  Copyright (C) 1999-2001 Michael B. Rash (mbr@cipherdyne.com)
*
*  License (GNU Public License):
*
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
*     USA
******************************************************************************
*
*  $Id$
*/

/* INCLUDES *****************************************************************/
#include "psad.h"

/* PROTOTYPES ***************************************************************/
static void parse_config(
    char *config_file,
    char *psadfifo_file,
    char *fwdata_file,
    char *fw_msg_search,
    char *kmsgsd_pid_file
);

/* MAIN *********************************************************************/
int main(int argc, char *argv[]) {
    char psadfifo_file[MAX_PATH_LEN+1];
    char fwdata_file[MAX_PATH_LEN+1];
    char config_file[MAX_PATH_LEN+1];
    char fw_msg_search[MAX_PATH_LEN+1];
    char kmsgsd_pid_file[MAX_PATH_LEN+1];
    int fifo_fd, fwdata_fd;  /* file descriptors */
    char buf[MAX_LINE_BUF+1];
    int numbytes;
#ifdef DEBUG
    int fwlinectr = 0;
#endif

#ifdef DEBUG
    printf(" ... Entering DEBUG mode ...\n");
    printf(" ... Firewall messages will be written to both ");
    printf("STDOUT _and_ to fwdata\n\n");
#endif

    /* handle command line arguments */
    if (argc == 1) {
        /* nothing but the program name was specified on the command line */
        strcpy(config_file, CONFIG_FILE);
    } else if (argc == 2) {
        /* the path to the config file was supplied on the command line */
        strcpy(config_file, argv[1]);
    } else {
        printf(" ... You may only specify the path to a single config file:  ");
        printf("Usage:  kmsgsd <configfile>\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf(" ... parsing config_file: %s\n", config_file);
#endif
    /* parse the config file */
    parse_config(config_file, psadfifo_file,
        fwdata_file, fw_msg_search, kmsgsd_pid_file);

    /* make sure there isn't another kmsgsd already running */
    check_unique_pid(kmsgsd_pid_file, "kmsgsd");

#ifndef DEBUG
    /* become a daemon */
    daemonize_process(kmsgsd_pid_file);
#endif

    /* start doing the real work now that the daemon is running and
     * the config file has been processed */

    /* open the psadfifo named pipe.  Note that we are opening the pipe
     * _without_ the O_NONBLOCK flag since we want the read on the file
     * descriptor to block until there is something new in the pipe. */
    if ((fifo_fd = open(psadfifo_file, O_RDONLY)) < 0) {
        perror(" ... @@@ Could not open psadfifo");
        exit(EXIT_FAILURE);  /* could not open psadfifo named pipe */
    }

    /* open the fwdata file in append mode so we can write messages from
     * the pipe into this file. */
    if ((fwdata_fd = open(fwdata_file, O_CREAT|O_WRONLY|O_APPEND, 0600)) < 0) {
        perror(" ... @@@ Could not open the fwdata_file");
        exit(EXIT_FAILURE);  /* could not open fwdata file */
    }

    /* MAIN LOOP:
     * Read data from the pipe indefinitely (we opened it _without_
     * O_NONBLOCK) and write it to the fwdata file if it is a firewall message
     */
    while ((numbytes = read(fifo_fd, buf, MAX_LINE_BUF)) >= 0) {
        if ((strstr(buf, "Packet log") != NULL ||
            (strstr(buf, "MAC") != NULL && strstr(buf, "IN") != NULL)) &&
            (strstr(buf, fw_msg_search))) {

            if (write(fwdata_fd, buf, numbytes) < 0)
                exit(EXIT_FAILURE);  /* could not write to the fwdata file */
#ifdef DEBUG
            buf[numbytes] = '\0';
            puts(buf);
            fwlinectr++;
            if (fwlinectr % 50 == 0)
                printf(" ... Processed %d firewall lines.\n", fwlinectr);
#endif
        }
    }

    /* these statements don't get executed, but for completeness... */
    close(fifo_fd);
    close(fwdata_fd);

    exit(EXIT_SUCCESS);
}
/******************** end main ********************/

static void parse_config(char *config_file, char *psadfifo_file,
    char *fwdata_file, char *fw_msg_search, char *kmsgsd_pid_file)
{
    FILE *config_ptr;         /* FILE pointer to the config file */
    int linectr = 0;
    char config_buf[MAX_LINE_BUF];
    char *index;

    if ((config_ptr = fopen(config_file, "r")) == NULL) {
        perror(" ... @@@ Could not open config file");
        exit(EXIT_FAILURE);
    }

    /* increment through each line of the config file */
    while ((fgets(config_buf, MAX_LINE_BUF, config_ptr)) != NULL) {
        linectr++;
        /* set the index pointer to the beginning of the line */
        index = config_buf;

        /* advance the index pointer through any whitespace
         * at the beginning of the line */
        while (*index == ' ' || *index == '\t') index++;

        /* skip comments and blank lines, etc. */
        if ((*index != '#') && (*index != '\n') &&
                (*index != ';') && (index != NULL)) {

            find_char_var("PSAD_FIFO ", psadfifo_file, index);
            find_char_var("FW_DATA ", fwdata_file, index);
            find_char_var("FW_MSG_SEARCH ", fw_msg_search, index);
            find_char_var("KMSGSD_PID_FILE ", kmsgsd_pid_file, index);
        }
    }
    fclose(config_ptr);
#ifdef DEBUG
    printf(" ... PSAD_FIFO: %s\n", psadfifo_file);
    printf(" ... FW_DATA: %s\n", fwdata_file);
    printf(" ... FW_MSG_SEARCH: %s\n", fw_msg_search);
    printf(" ... KMSGSD_PID_FILE: %s\n", kmsgsd_pid_file);
#endif
    return;
}