#include "legato.h"
#include "le_tty.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions          */
#include <errno.h>   /* ERROR Number Definitions           */
#include <sys/sysinfo.h>
#include "interfaces.h"

// Reference to the FD Monitor
int SerialPortFd;
static le_fdMonitor_Ref_t legaRead;
ssize_t bytesRead, bytes;
int i, len;
char ascii;
le_result_t res;
static int data = 0;

/* variables */
#define ASCII_VAR_RES   "/home/ascii" //un path virtuel .. for secure ....
#define HEX_VAR_RES   "/home/hex"
/* settings*/
#define TARGET_DATA_SET_RES "/home/data"
//reference to AVC event handler
le_avdata_SessionStateHandlerRef_t  avcEventHandlerRef = NULL;
//reference to AVC Session handler
le_avdata_RequestSessionObjRef_t sessionRef = NULL;


//This function is called whenever push has been performed successfully in AirVantage server
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status,
    void* contextPtr
)
{
    switch (status)
    {
        case LE_AVDATA_PUSH_SUCCESS:
            LE_INFO("Legato assetdata push successfully");
            break;
        case LE_AVDATA_PUSH_FAILED:
            LE_INFO("Legato assetdata push failed");
            break;
    }
}

static void legaReadEventHandler(int fd,short events)
{
    if (events & POLLIN)
    {
        char buff[1000];
        char hex[1000];

// Read the bytes, retrying if interrupted by a signal.
        LE_INFO("======== bytesRead is Ready ========");
        bytes = read(fd, buff, sizeof(buff));
        LE_INFO("Number of data = %d", bytes);

		            if (bytes == 0)
   		             {
        		           exit(EXIT_SUCCESS);
    	             }
   		          if (bytes < 0)
                   {
        	             if (errno != EINTR)
       	                  {
            		             LE_FATAL("read() failed: %m");
                          }
                   }
                if (bytes > 0)
                   {
                       for(i = 0; i<bytes; i++)
                           {
                              sprintf(hex+i*2, "%02X", buff[i]); // to convert in HEX FORMAT
                              buff[bytes] = '\0';
                           }
                       LE_INFO("Hex Data : %s", hex);
                       LE_INFO("Ascii Data : %s", buff);
                   }
         //Sets an asset data to a string value.
         le_result_t resultSetStore1 = le_avdata_SetString(ASCII_VAR_RES, buff);
         if (LE_FAULT == resultSetStore1)
         {
             LE_ERROR("Error in setting ASCII_VAR_RES");
         }
         le_result_t resultSetStore2 = le_avdata_SetString(HEX_VAR_RES, hex);
         if (LE_FAULT == resultSetStore2)
         {
             LE_ERROR("Error in setting HEX_VAR_RES");
         }
         if (NULL != avcEventHandlerRef)
         {
             le_result_t resultPusAsciihData;
             //Push asset data to the server
             resultPusAsciihData = le_avdata_Push(ASCII_VAR_RES, PushCallbackHandler, NULL);
             if (LE_FAULT == resultPusAsciihData)
             {
                 LE_ERROR("Error pushing ASCII_VAR_RES");
             }
             le_result_t resultPushHexData;
             resultPushHexData = le_avdata_Push(HEX_VAR_RES, PushCallbackHandler, NULL);
             if (LE_FAULT == resultPushHexData)
             {
                 LE_ERROR("Error pushing HEX_VAR_RES");
             }
         }

    }
    else if ((events & POLLERR) || (events & POLLHUP) || (events & POLLRDHUP))
    {
        LE_ERROR("Error on read file descriptor.");
    }
}
//Setting data handler.
static void DataSettingHandler
(
    const char* path,
    le_avdata_AccessType_t accessType,
    le_avdata_ArgumentListRef_t argumentList,
    void* contextPtr
)

// DataSettingHandler
{
    le_result_t resultGetInt = le_avdata_GetInt(TARGET_DATA_SET_RES, &data);
    if (LE_FAULT == resultGetInt)
    {
        LE_ERROR("Error in getting latest TARGET_DATA_SET_RES");
    }
    LE_INFO(" DATA RECEIVED: %d", data);

    if (data == 99)
    {
          le_result_t baud = le_tty_SetBaudRate(SerialPortFd, LE_TTY_SPEED_115200);
          if (baud == LE_OK)
          {
            LE_INFO(" The baudrate value is CHANGED successfully");
          }
          else
          {
            LE_ERROR("Changing the baudrate Failed");
          }
    }
    else
    {
          LE_ERROR("Reception Failed");
    }
}

// status of the session created
static void avcStatusHandler
(
    le_avdata_SessionState_t updateStatus,
    void* contextPtr
)
{
    switch (updateStatus)
    {
        case LE_AVDATA_SESSION_STARTED:
            LE_INFO("Legato session started successfully");
            break;
        case LE_AVDATA_SESSION_STOPPED:
            LE_INFO("Legato session stopped");
            break;
    }
}
COMPONENT_INIT /******************************************************************************************************/
{
    LE_INFO("======== Starting Tty api Test ========");

    SerialPortFd =  le_tty_Open("/dev/ttyHS0", O_RDWR | O_NOCTTY | O_NDELAY );
                    LE_FATAL_IF(SerialPortFd == -1, "open failed with errno %d (%m)", errno);
                    le_tty_SetFraming(SerialPortFd, 'N', 8, 1);
                    le_tty_SetBaudRate(SerialPortFd, LE_TTY_SPEED_9600);
    // Create a File Descriptor Monitor object for the serial port's file descriptor.
    legaRead = le_fdMonitor_Create("legaRead", SerialPortFd, legaReadEventHandler, POLLIN);
    LE_INFO("Start Legato AssetDataApp"); //Start an AVC Session:
    //Start AVC Session
    //Register AVC handler//Add handler function for EVENT 'le_avdata_SessionState'
    avcEventHandlerRef = le_avdata_AddSessionStateHandler(avcStatusHandler, NULL);//(1)This event provides information on AV session state changes
    //Request AVC session. Note: AVC handler must be registered prior starting a session
    le_avdata_RequestSessionObjRef_t sessionRequestRef = le_avdata_RequestSession();//(2)Request the avcServer to open a session.
    if (NULL == sessionRequestRef)
      {
        LE_ERROR("AirVantage Connection Controller does not start.");
      }
    else
      {
        sessionRef=sessionRequestRef;
        LE_INFO("AirVantage Connection Controller started.");
      }
    // [CreateResources]
    LE_INFO("Create instances AssetData ");
    le_result_t resultCreateData;
    resultCreateData = le_avdata_CreateResource(ASCII_VAR_RES,LE_AVDATA_ACCESS_VARIABLE);//(3)
    if (LE_FAULT == resultCreateData)
      {
          LE_ERROR("Error in creating ASCII_VAR_RES");
      }
    le_result_t resultCreateHexData;
    resultCreateHexData = le_avdata_CreateResource(HEX_VAR_RES,LE_AVDATA_ACCESS_VARIABLE);
    if (LE_FAULT == resultCreateHexData)
      {
          LE_ERROR("Error in creating HEX_VAR_RES");
      }
    le_result_t resultCreateTargetData;
    resultCreateTargetData= le_avdata_CreateResource(TARGET_DATA_SET_RES,LE_AVDATA_ACCESS_SETTING);
    if (LE_FAULT == resultCreateTargetData)
    {
        LE_ERROR("Error in creating TARGET_DATA_SET_RES");
    }

    //Register handler for Settings
    LE_INFO("Register handler of paths");
    le_avdata_AddResourceEventHandler(TARGET_DATA_SET_RES,DataSettingHandler, NULL);

}
