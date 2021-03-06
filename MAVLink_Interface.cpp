#define S_FUNCTION_NAME  MAVLink_Interface
#define S_FUNCTION_LEVEL 2

#ifdef _WIN32
        #include <winsock2.h>
#endif
       
#include <windows.h>
#include "simstruc.h"
#include "include/mavlink_types.h"
#include "include/common/common.h"
#include "APMMAVLink/DataLink/DataLink.h"
#include "APMMAVLink/DataLink/SerialDataLink.h"
#include "APMMAVLink/DataLink/XBeeAPIDataLink.h"
#include "APMMAVLink/DataLink/TCPDataLink.h"
#include "APMMAVLink/DataLink/MAVLink.h"
#include "APMMAVLink/DataLink/MAVLinkComponent.h"


#define COM_PORT_IDX 0
#define COM_PORT(S) ssGetSFcnParam(S,COM_PORT_IDX)

#define COM_BUAD_IDX 1
#define COM_BAUD(S) ssGetSFcnParam(S,COM_BUAD_IDX)

#define IP_ADDRESS_IDX 2
#define IP_ADDRESS(S) ssGetSFcnParam(S,IP_ADDRESS_IDX)

#define IP_PORT_IDX 3
#define IP_PORT(S) ssGetSFcnParam(S,IP_PORT_IDX)

#define MY_SYS_ID_IDX  4
#define MY_SYS_ID(S) ssGetSFcnParam(S,MY_SYS_ID_IDX)

#define MY_COM_ID_IDX  5
#define MY_COM_ID(S) ssGetSFcnParam(S,MY_COM_ID_IDX)
        
#define TARGET_COM_IDX  6
#define TARGET_COM(S) ssGetSFcnParam(S,TARGET_COM_IDX)

#define MSG_IN_IDX  7
#define MSG_IN(S) ssGetSFcnParam(S,MSG_IN_IDX)

#define MSG_OUT_IDX  8
#define MSG_OUT(S) ssGetSFcnParam(S,MSG_OUT_IDX)


#define NPARAMS   9


#if !defined(MATLAB_MEX_FILE)
    #error This_file_can_be_used_only_during_simulation_inside_Simulink
#endif
        
 
int count = 0;
std::map<int, DataLink*> linkMap;
std::map<int, MAVLink*> mavlinkMap;
HANDLE hThreadRead = NULL;

bool looping = true;
bool finishedLooping = false;
bool streamsSetup = false;
int_T mavlink_msg_decode(SimStruct* S);

DWORD WINAPI mavThreadRead(LPVOID lpParam);
void mavlink_setup_streams(SimStruct* S);

void printMessage(SimStruct *S,static const char * msg) {
    int index = *(int*)ssGetDWork(S,0);
    printf("MAVLink_Interface #%d: %s\n", index, msg);
}
void printError(SimStruct *S,static const char * msg) {
    printMessage(S, msg);
    ssSetErrorStatus(S, msg);
}

#undef MDL_CHECK_PARAMETERS 

static void mdlInitializeSizes(SimStruct *S) {
    int_T nInputPorts  = 0;  /* number of input ports  */
    int_T nOutputPorts = 0;  /* number of output ports */
    
    uint8_T *ptrMsg_id_in;
    uint8_T *ptrMsg_id_out;
  
    ssSetNumSFcnParams(S, NPARAMS);  /* Number of expected parameters */
#if defined(MATLAB_MEX_FILE)
    if (ssGetNumSFcnParams(S) == ssGetSFcnParamsCount(S)) {
//        mdlCheckParameters(S);
        if (ssGetErrorStatus(S) != NULL) {
            return;
        }
    } else {
        return; /* Parameter mismatch will be reported by Simulink */
    }
#endif

    ssSetSFcnParamTunable(S,COM_PORT_IDX,false);
    ssSetSFcnParamTunable(S,COM_BUAD_IDX,false);
    ssSetSFcnParamTunable(S,IP_ADDRESS_IDX,false);
    ssSetSFcnParamTunable(S,IP_PORT_IDX,false);
    ssSetSFcnParamTunable(S,MY_SYS_ID_IDX,false);
    ssSetSFcnParamTunable(S,MY_COM_ID_IDX,false);
    ssSetSFcnParamTunable(S,TARGET_COM_IDX,false);
    ssSetSFcnParamTunable(S,MSG_IN_IDX,false);
    ssSetSFcnParamTunable(S,MSG_OUT_IDX,false);
    
    
    ptrMsg_id_in = (uint8_T*) mxGetData(MSG_IN(S));
    ptrMsg_id_out =  (uint8_T*) mxGetData(MSG_OUT(S)); /* pointer to MSG_OUT_IDs */
    nInputPorts = (int_T) mxGetNumberOfElements(MSG_IN(S)); /* set Num of input port according to the MAVlink MSG */
    nOutputPorts = (int_T) mxGetNumberOfElements(MSG_OUT(S)); /* set Num of output port according to the MAVlink MSG */

    /* Register the number and type of states the S-Function uses */
    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, 0);   /* number of discrete states             */

    if (!ssSetNumInputPorts(S, nInputPorts)) return;    
    
    for (int ii = 0; ii < nInputPorts; ii++) {        
        switch ( *(ptrMsg_id_in + ii) ){
            case MAVLINK_MSG_ID_SET_MODE:{
                ssSetInputPortWidth(S, ii, 1); /* Mode_ID*/
                break;
            }
            case MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE:{
                ssSetInputPortWidth(S, ii, 8); /* RC Input channels 1-8 */
                break;
            }
            case MAVLINK_MSG_ID_SET_ROLL_PITCH_YAW_THRUST:{
                ssSetInputPortWidth(S, ii, 4); /* roll, pitch, yaw, thrust */
                break;
            }
            case MAVLINK_MSG_ID_REQUEST_DATA_STREAM:{
                ssSetInputPortWidth(S, ii, 3); /* Stream_ID, freq, start_stop */
                break;
            }
            case MAVLINK_MSG_ID_ACTION:{
                ssSetInputPortWidth(S, ii, 1); /* Action ID */
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE:{
                ssSetInputPortWidth(S, ii, 6); /* (usec), roll, pitch, yaw, roll rate, pitch rate, yaw rate */
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW:{
                ssSetInputPortWidth(S, ii, 5); /*  (usec), (fix_type), lat, lon, alt, (eph), (epv), v, hdg */
                break;
            }
            case MAVLINK_MSG_ID_VFR_HUD:{
                ssSetInputPortWidth(S, ii, 6); /* airspeed, groundspeed, heading, throttle, altitude, climbrate */
                break;
            }
            case MAVLINK_MSG_ID_WAYPOINT:{
                ssSetInputPortDimensionInfo(S, ii, DYNAMIC_DIMENSION);
//                 ssSetInputPortWidth(S, ii, 10); /* num, frame, command, param 1-4, position x y z */
                break;
            }
            default:
                ssSetInputPortWidth(S, ii, 1);
                break;
        } /* switch end*/        
     } 
    
    if (!ssSetNumOutputPorts(S, nOutputPorts)) return;

    for (int ii = 0; ii < nOutputPorts; ii++) {        
        switch ( *(ptrMsg_id_out + ii) ){
            case MAVLINK_MSG_ID_HEARTBEAT:{
                ssSetOutputPortWidth(S, ii, 1); /* heart beat */
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE:{
                ssSetOutputPortWidth(S, ii, 6); /* roll, pitch, yaw, p, q, r */
                break;
            }
            case MAVLINK_MSG_ID_RAW_IMU:{
                ssSetOutputPortWidth(S, ii, 9); /* acc(xyz) gyro(xyz) mag(xyz) */
                break;
            }
            case MAVLINK_MSG_ID_VFR_HUD:{
                ssSetOutputPortWidth(S, ii,6); /* airspeed, groundspeed, heading, throttle, altitude, climbrate */
                break;
            }
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:{
                ssSetOutputPortWidth(S, ii, 6); /*  lat, lon, alt, vx, vy, vz */
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW: {
                ssSetOutputPortWidth(S, ii, 5); /*  lat, lon, alt, gs, trk */
                break;
            }
            case MAVLINK_MSG_ID_RC_CHANNELS_SCALED:{
                ssSetOutputPortWidth(S, ii, 9); /* s1, s2, s3, s4, s5, s6, s7, s8, rssi */
                break;
            }
            case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:{
                ssSetOutputPortWidth(S, ii, 8); /* s1, s2, s3, s4, s5, s6, s7, s8 */
                break;
            }
            case MAVLINK_MSG_ID_SYS_STATUS: {
                ssSetOutputPortWidth(S, ii, 7); /* mode, nav_mode, status, load, vbat, battery_remaining, packet_drop */
                break;
            }
            default:
                ssSetOutputPortWidth(S, ii, 1);
                break;
        }       
     } 

    ssSetNumDWork(S, 1);
    ssSetDWorkWidth(S, 0, 1);
    ssSetNumPWork(S, nOutputPorts); // pointer buffer for each output
    ssSetNumSampleTimes(S, 1);   /* number of sample times   */
    ssSetSimStateCompliance(S, USE_DEFAULT_SIM_STATE);

}

static void mdlInitializeSampleTimes(SimStruct *S) {
    ssSetSampleTime(S, 0, CONTINUOUS_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}

#if defined(MATLAB_MEX_FILE)
#define MDL_SET_INPUT_PORT_DIMENSION_INFO
static void mdlSetInputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo) {
    if(!ssSetInputPortDimensionInfo(S, port, dimsInfo)) return;
}

# define MDL_SET_OUTPUT_PORT_DIMENSION_INFO
static void mdlSetOutputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo) {}
#endif

#define MDL_START  /* Change to #undef to remove function */
#ifdef MDL_START
static void mdlStart(SimStruct *S) {
    uint8_T* ptrMsg_id_in = (uint8_T*)mxGetData(MSG_IN(S));
    int_T nInputPorts = (int_T)mxGetNumberOfElements(MSG_IN(S));
    
    uint8_T* ptrMsg_id_out = (uint8_T*)mxGetData(MSG_OUT(S));
    int_T nOutputPorts = (int_T)mxGetNumberOfElements(MSG_OUT(S));
    
    real_T** ppPwork_data = (real_T**) ssGetPWork(S);
   
    /*Allocate memory for Output data*/
    for (int ii = 0; ii < nOutputPorts; ii++) {
        /* msg_id =  *(pMsg_id + ii); */
        ppPwork_data[ii] = (real_T*) calloc(ssGetOutputPortWidth(S,ii), sizeof(real_T));
    } 
    
    int *myIndex = (int*)ssGetDWork(S,0);
    int index = count++;
    *myIndex = index;
    char msg[128];
    
    /* Setup Serial port */
    char_T strCOM[sizeof("COM00")];
    mxGetString(COM_PORT(S),strCOM,sizeof(strCOM));
    int baud = (int)*(real_T*) mxGetData(COM_BAUD(S));
    char_T strIP[sizeof("xxx.xxx.xxx.xxx")];
    mxGetString(IP_ADDRESS(S),strIP,sizeof(strIP));
    int port = (int)*(real_T*) mxGetData(IP_PORT(S));
    int mySys = (int)*(real_T*) mxGetData(MY_SYS_ID(S));
    int myComp = (int)*(real_T*) mxGetData(MY_COM_ID(S));
    char_T strComp[256];
    mxGetString(TARGET_COM(S),strComp,sizeof(strComp));
    
    /* Begin setting up the datalink object */
    DataLink * startLink = NULL;
    
    /* Use component database (components.txt) to associate addresses
     *  with system/component IDs */
    MAVLinkComponent *myComponent = new MAVLinkComponent(strComp);
    
    bool isSerial = false;
    
    /* If the component was found, try to make a datalink object */
    if (myComponent->isValidComponent()) {
        /* are we a serial port? */
        if (strcmp(strCOM,"") != 0) {
            /* Make a serial data object */
            startLink = new SerialDataLink(strCOM, baud);
  //          ((SerialDataLink*)startLink)->setNumberOfStopBits(TWOSTOPBITS);
            
            /* If we're an API connection, overright the link object */
             if (myComponent->getPhysicalAddressHigh() != 0 && myComponent->getPhysicalAddressLow() != 0) {
                startLink = new XBeeAPIDataLink(*(SerialDataLink*)startLink,myComponent->getPhysicalAddressHigh(),myComponent->getPhysicalAddressLow());
 //               ((SerialDataLink*)startLink)->setNumberOfStopBits(TWOSTOPBITS);
             }
            isSerial = true;
        }
        
        /* are we a network port? */
        if (strcmp(strIP,"") != 0) {
            startLink = new TCPDataLink(strIP, port, false);
            isSerial = false;
        }
    } else {
        sprintf(msg,"No valid component specified for '%s'", strComp);
        printError(S,msg);
        return;
    }
    
    /* If we didn't create anything, quit out */
    if (startLink == NULL) {
        sprintf(msg,"No valid connections specified for '%s'", strComp);
        printError(S,msg);
        return;
    }
    
    /* Assuming alls gone well, create a MAVLink object */
    MAVLink * startMAV;
    
    /* Keep track of whether we're an API or not */
    bool zigbeeAPI = false;
    
    /* loop over all current datalinks to see if we need to re-use any
     *  Reuse checks if identifiers already exist (i.e. COM5) and if so
     *  associates this instances MAVLink object with that instead of creating
     *  a new one */
    for (int i = 0; i<linkMap.size(); i++) {
        /* compare identifiers */
        if (strcmp(linkMap[i]->getIdentifier(), startLink->getIdentifier()) == 0) {
            /* give the user some info */
            sprintf(msg,"%s already in open, reusing...", startLink->getIdentifier());
            printMessage(S,msg);
            
            /* are we an API object */
            if ((myComponent->getPhysicalAddressHigh() == 0 && myComponent->getPhysicalAddressLow() == 0) || !isSerial) {
                /* if not, make an object around previous identifier */
                startMAV = new MAVLink(mySys,myComp,linkMap[i]);
                
                /* remove our link as we don't need it anymore */
                delete startLink;
                startLink = NULL;
            } else {
                /* if we're API, overwrite our link with a new one
                 *  this will use the same serial link but have a different
                 *  address */
                zigbeeAPI = true;
                delete startLink;
                startLink = new XBeeAPIDataLink(*(SerialDataLink*)linkMap[i],myComponent->getPhysicalAddressHigh(),myComponent->getPhysicalAddressLow());
                ((SerialDataLink*)startLink)->setNumberOfStopBits(TWOSTOPBITS);
            }
        }
    }
    
    /* assuming we're not reusing, add our link to the map and create
     *  a MAVLink object */
    if (startLink != NULL) {
        linkMap[linkMap.size()] = startLink;
        
        /* if we're in API (and reusing), dont try connecting again */
        if (!zigbeeAPI) {
            if((linkMap[linkMap.size()-1])->connect()) {
                sprintf(msg,"Connected to %s",(linkMap[linkMap.size()-1])->getIdentifier());
                printMessage(S,msg);
            } else {
                sprintf(msg,"Failed to connect to %s",(linkMap[linkMap.size()-1])->getIdentifier());
                printError(S,msg);
                return;
            }
        }
        
        /* create a new MAVLink object */
        startMAV = new MAVLink(mySys,myComp,linkMap[linkMap.size()-1]);
    }
    
    /* check for duplicate components and caution the user (we're not sure how
     *  this will behave! */
    for (int i = 0; i<index; i++) {
        if (mavlinkMap[i]->getTargetSystem() == myComponent->getSystemID()
                && mavlinkMap[i]->getTargetComponent() == myComponent->getComponentID()) {
            sprintf(msg,"Component %d:%d already exists in #%d (bad things may happen!)",myComponent->getSystemID(),  myComponent->getComponentID(), i);
            printMessage(S,msg);
        }
    }
    
    /* set our target system and save the MAVLink object */
    startMAV->setTargetComponent(myComponent->getSystemID(), myComponent->getComponentID());
    mavlinkMap[index] = startMAV;

    /* set up our streams (to default APM ground station values)*/
    if (!streamsSetup) {
        mavlink_setup_streams(S);
        Sleep(100);
        streamsSetup = true;
    }
    
    /* let thread loop execute indefinately (until terminate is called) */
    looping = true;
    
    /* create our MAVLink thread */
    if (hThreadRead == NULL) {
        sprintf(msg,"Creating MAVLink Thread");
            printMessage(S,msg);
        hThreadRead = CreateThread( NULL, 0, 
           mavThreadRead, S, THREAD_TERMINATE, NULL);
    }
}
#endif /*  MDL_START */


/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector(s),
 *    ssGetOutputPortSignal.
 */
static void mdlOutputs(SimStruct *S, int_T tid) {
    int_T nOutputPorts = (int_T)mxGetNumberOfElements(MSG_OUT(S));
    int_T nOutputWidth; 
    real_T** ppPwork_data = (real_T**) ssGetPWork(S);
    
    real_T** outputArray;
    
    outputArray = (real_T**)calloc(nOutputPorts,sizeof(real_T*));
    
    for (int ii = 0; ii < nOutputPorts; ii++){
        *(outputArray+ii) = ssGetOutputPortRealSignal(S,ii);
        nOutputWidth = ssGetOutputPortWidth(S,ii);
        for (int jj = 0; jj < nOutputWidth; jj++){
            *(*(outputArray+ii) + jj) = ppPwork_data[ii][jj]; /* assign output values*/
        }        
    }
    free(outputArray);
   
//     looping = true;
} /* end mdlOutputs */


#define MDL_UPDATE  /* Change to #undef to remove function */
#if defined(MDL_UPDATE)
  /* Function: mdlUpdate ======================================================
   * Abstract:
   *    This function is called once for every major integration time step.
   *    Discrete states are typically updated here, but this function is useful
   *    for performing any tasks that should only take place once per
   *    integration step.
   */
static void mdlUpdate(SimStruct *S, int_T tid) {
    int_T nInputPorts = ssGetNumInputPorts(S);
    int_T nInputPortWidth;
    uint8_T* ptrMsg_id_in = (uint8_T*) mxGetData(MSG_IN(S));
    InputRealPtrsType pInput;// = ssGetInputPortRealSignalPtrs(S,port);
    mavlink_message_t msg;
    mavlink_message_t count; // counts for waypoint numbers
    uint16_T msg_len, buf_len, chksum;
    uint8_T* ptrBuf;
    real_T* ptrLen = (real_T*) ssGetRWork(S);
    int myIndex = *(uint8_T*) ssGetDWork(S,0);
    
    mavlink_msg_decode(S);
//    mavlinkMap[myIndex]->receiveMessage();

    for (int_T index = 0; index < nInputPorts; index++){             
        switch (*(ptrMsg_id_in+index)) {
            case MAVLINK_MSG_ID_SET_MODE: {
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                uint8_T mode = (uint8_T) (*pInput[0]);
                mavlinkMap[myIndex]->sendMode(mode);
                break;
            }
            case MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE: {
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                uint16_T chan1_raw = (uint16_T) (*pInput[0]);
                uint16_T chan2_raw = (uint16_T) (*pInput[1]);
                uint16_T chan3_raw = (uint16_T) (*pInput[2]);
                uint16_T chan4_raw = (uint16_T) (*pInput[3]);
                uint16_T chan5_raw = (uint16_T) (*pInput[4]);
                uint16_T chan6_raw = (uint16_T) (*pInput[5]);
                uint16_T chan7_raw = (uint16_T) (*pInput[6]);
                uint16_T chan8_raw = (uint16_T) (*pInput[7]); 
                mavlinkMap[myIndex]->sendRCOverride(chan1_raw, chan2_raw, chan3_raw, chan4_raw, chan5_raw, chan6_raw, chan7_raw, chan8_raw);
                break;
            }
            case MAVLINK_MSG_ID_SET_ROLL_PITCH_YAW_THRUST: {
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                float roll = (float) (*pInput[0]);
                float pitch = (float) (*pInput[1]);
                float yaw = (float) (*pInput[2]);
                float throttle = (float) (*pInput[3]);
                mavlinkMap[myIndex]->sendAttitudeCommand(roll, pitch, yaw, throttle);
                break;
            }
            case MAVLINK_MSG_ID_REQUEST_DATA_STREAM: {                
//                pInput = ssGetInputPortRealSignalPtrs(S,index);
//                uint8_T stream_id = (uint8_T) (*pInput[0]);
//                uint16_T msg_rate = (uint16_T) (*pInput[1]);
//                uint8_T start_stop = (uint8_T) (*pInput[2]); 
//                msg_len = mavlink_msg_request_data_stream_pack(255, 0, &msg, 1, 0, stream_id, msg_rate, start_stop);
//                ptrBuf = (uint8_T*) ssGetDWork(S,index);
//                
//                chksum = crc_calculate(ptrBuf+3,msg_len-4);       /* checksum just for the msg in the sending buffer */ 
//                buf_len = mavlink_msg_to_send_buffer(ptrBuf, &msg);   
//                if (chksum == crc_calculate(ptrBuf+3,msg_len-4)){ /*check if the content has been changed */
//                    if (ptrLen[index] == -1){
//                        ptrLen[index] = 0;
//                    } 
//                } else {
//                    ptrLen[index] = (real_T) buf_len;
//                }
                
                break;
            }
            case MAVLINK_MSG_ID_ACTION:{
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE:{
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                float roll = (float) (*pInput[0]);
                float pitch = (float) (*pInput[1]);
                float yaw = (float) (*pInput[2]);
                float p = (float) (*pInput[3]);
                float q = (float) (*pInput[4]);
                float r = (float) (*pInput[5]);
                mavlinkMap[myIndex]->sendAttitude(roll, pitch, yaw, p, q, r);
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW:{
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                float lat = (float) (*pInput[0]);
                float lon = (float) (*pInput[1]);
                float alt = (float) (*pInput[2]);
                float vel = (float) (*pInput[3]);
                float hdg = (float) (*pInput[4]);
                mavlinkMap[myIndex]->sendRawGPS(3, lat, lon, alt, 0.0, 0.0, vel, hdg);
                break;
            }
            case MAVLINK_MSG_ID_VFR_HUD:{
                pInput = ssGetInputPortRealSignalPtrs(S,index);
                float airspeed = (float) (*pInput[0]);
                float groundspeed = (float) (*pInput[1]);
                int16_t heading = (uint16_t) (*pInput[2]);
                uint16_t throttle = (uint16_t) (*pInput[3]);
                float alt = (float) (*pInput[4]);
                float climb = (float) (*pInput[5]);
                mavlinkMap[myIndex]->sendVFRHUD(airspeed, groundspeed, heading, throttle, alt, climb);
                break;
            }
            case MAVLINK_MSG_ID_WAYPOINT:{
                break;
            }
            default:
                break;
        }
    }
}
#endif /* MDL_UPDATE */
static void mdlTerminate(SimStruct *S) {
    
    looping = false;
    int i = 0;
    if (hThreadRead != NULL && TerminateThread(hThreadRead, -1) == 0 ) {
        static char msg[256];
        sprintf(msg, "Could not stop thread (Error %d)",GetLastError());
        printMessage(S,msg);
    }
    CloseHandle(hThreadRead);
    hThreadRead = NULL;
//    int myIndex = *(uint8_T*) ssGetDWork(S,0);
    for (int i = 0; i<mavlinkMap.size(); i++) {
        delete mavlinkMap[i];
        mavlinkMap[i] = NULL;
    }
    
    
    for (int i = 0; i<linkMap.size(); i++) {
        if (linkMap[i]->isConnected() && linkMap[i]->disconnect()) {
            static char msg[256];
            sprintf(msg, "%s closed",linkMap[i]->getIdentifier());
            printMessage(S,msg);
        }
        delete linkMap[i];
        linkMap[i] = NULL;
    }
    count = 0;
    mavlinkMap.clear();
    linkMap.clear();
}

int k = 0;
DWORD WINAPI mavThreadRead(LPVOID lpParam) {
    Sleep(100);
    while (looping) {
        for (int i = 0; i<mavlinkMap.size(); i++) {
            int t = mavlinkMap[i]->receiveMessage();
            if (t<0) {
                printf("%d packets dropped!\n",-t);
            }
            t = mavlinkMap[i]->sendMessages();
        } 
    }
    return 0;
}

void mavlink_setup_streams(SimStruct* S) {   
    int_T nOutputPorts = (int_T)mxGetNumberOfElements(MSG_OUT(S));
    real_T** ppPwork_data = (real_T**) ssGetPWork(S);
    
    uint8_T* ptrMsg_id_out = (uint8_T*) mxGetData(MSG_OUT(S));
    int8_T index;

    int myIndex = *(uint8_T*) ssGetDWork(S,0);

        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_ALL, 0, 0);
        mavlinkMap[myIndex]->sendMessages();
        Sleep(100);
    
  //  for (int_T index = 0; index < nOutputPorts; index++){
        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_EXTENDED_STATUS, 5, 1);
        mavlinkMap[myIndex]->sendMessages();
        Sleep(100);
        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_POSITION, 10, 1);
        mavlinkMap[myIndex]->sendMessages();
        Sleep(100);
        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_EXTRA1, 15, 1);
        mavlinkMap[myIndex]->sendMessages();
        Sleep(100);
        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_EXTRA2, 10, 1);
        mavlinkMap[myIndex]->sendMessages();
        Sleep(100);
//        mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_RAW_SENSORS, 10, 1);
//        mavlinkMap[myIndex]->sendMessages();
//        Sleep(100);
 //       mavlinkMap[myIndex]->sendRequestStream(MAV_DATA_STREAM_RC_CHANNELS, 0, 0);        
  //      mavlinkMap[myIndex]->sendMessages();
//        Sleep(100);
//    }
}

int_T mavlink_msg_decode(SimStruct* S) {   
    int_T nOutputPorts = (int_T)mxGetNumberOfElements(MSG_OUT(S));
    real_T** ppPwork_data = (real_T**) ssGetPWork(S);
    
    uint8_T* ptrMsg_id_out = (uint8_T*) mxGetData(MSG_OUT(S));
    int8_T index;

    int myIndex = *(uint8_T*) ssGetDWork(S,0);
    
    
    
    for (int_T index = 0; index < nOutputPorts; index++){    
        switch (*(ptrMsg_id_out+index)) {
            case MAVLINK_MSG_ID_HEARTBEAT:{
//                ppPwork_data[index][0] = ppPwork_data[index][0]+1;
                break;
            }
            case MAVLINK_MSG_ID_ATTITUDE: {
                float roll, pitch, yaw, p, q, r;
                if(mavlinkMap[myIndex]->getAttitude(roll, pitch, yaw, p, q, r)) {
                    ppPwork_data[index][0] = (real_T) roll;
                    ppPwork_data[index][1] = (real_T) pitch;
                    ppPwork_data[index][2] = (real_T) yaw;
                    ppPwork_data[index][3] = (real_T) p;
                    ppPwork_data[index][4] = (real_T) q;
                    ppPwork_data[index][5] = (real_T) r;
                } else {
//                    printMessage(S,"No attitude data available!");
                }
                break;
            }
            case MAVLINK_MSG_ID_RAW_IMU: {
/*                ppPwork_data[index][0] = (real_T) mavlink_msg_raw_imu_get_xacc(&msg);
                ppPwork_data[index][1] = (real_T) mavlink_msg_raw_imu_get_yacc(&msg);
                ppPwork_data[index][2] = (real_T) mavlink_msg_raw_imu_get_zacc(&msg);
                ppPwork_data[index][3] = (real_T) mavlink_msg_raw_imu_get_xgyro(&msg);
                ppPwork_data[index][4] = (real_T) mavlink_msg_raw_imu_get_ygyro(&msg);
                ppPwork_data[index][5] = (real_T) mavlink_msg_raw_imu_get_zgyro(&msg);
                ppPwork_data[index][5] = (real_T) mavlink_msg_raw_imu_get_xmag(&msg);
                ppPwork_data[index][6] = (real_T) mavlink_msg_raw_imu_get_ymag(&msg);
                ppPwork_data[index][7] = (real_T) mavlink_msg_raw_imu_get_zmag(&msg);*/
                break;
            }                
            case MAVLINK_MSG_ID_VFR_HUD: {
                float airspeed, groundspeed, alt, climb;
                int16_t heading;
                uint16_t throttle;
                if(mavlinkMap[myIndex]->getVFRHUD(airspeed, groundspeed, heading, throttle, alt, climb)) {
                    ppPwork_data[index][0] = (real_T) airspeed;
                    ppPwork_data[index][1] = (real_T) groundspeed;
                    ppPwork_data[index][2] = (real_T) heading;
                    ppPwork_data[index][3] = (real_T) throttle;
                    ppPwork_data[index][4] = (real_T) alt;
                    ppPwork_data[index][5] = (real_T) climb;
                } else {
//                    printMessage(S,"No VFR HUD data available!");
                }
                break;
            }
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                int32_t lat, lng, alt ;
                int16_t vx, vy, vz;
                if(mavlinkMap[myIndex]->getGlobalPositionInt(lat, lng, alt, vx, vy, vz)) {
                    ppPwork_data[index][0] = (real_T)((long double)lat)/10000000.0;
                    ppPwork_data[index][1] = (real_T)(lng/10000000.0);
                    ppPwork_data[index][2] = (real_T)(alt/1000.0);
                    ppPwork_data[index][3] = (real_T)(vx/100.0);
                    ppPwork_data[index][4] = (real_T)(vy/100.0);
                    ppPwork_data[index][5] = (real_T)(vz/100.0);
                } else {
//                    printMessage(S,"No GPS data available!");
                }
                break;
            }
            case MAVLINK_MSG_ID_GPS_RAW: {
                int fix;
                float lat, lng, alt, eph, epv, v, crs;
                if(mavlinkMap[myIndex]->getRawGPS(fix,lat, lng, alt, eph, epv, v, crs)) {
                    ppPwork_data[index][0] = (real_T)(lat);
                    ppPwork_data[index][1] = (real_T)(lng);
                    ppPwork_data[index][2] = (real_T)(alt);
                    ppPwork_data[index][3] = (real_T)(v);
                    ppPwork_data[index][4] = (real_T)(crs);
                }
                break;
            }
            case MAVLINK_MSG_ID_RC_CHANNELS_SCALED:{
                int16_t s1, s2, s3, s4, s5, s6, s7, s8;
                uint8_t rssi;
                if (mavlinkMap[myIndex]->getScaledServos(s1, s2, s3, s4, s5, s6, s7, s8, rssi)) {
                    ppPwork_data[index][0] = (real_T) s1;
                    ppPwork_data[index][1] = (real_T) s2;
                    ppPwork_data[index][2] = (real_T) s3;
                    ppPwork_data[index][3] = (real_T) s4;
                    ppPwork_data[index][4] = (real_T) s5;
                    ppPwork_data[index][5] = (real_T) s6;
                    ppPwork_data[index][6] = (real_T) s7;
                    ppPwork_data[index][7] = (real_T) s8;
                    ppPwork_data[index][8] = (real_T) rssi;
                } else {
//                    printMessage(S,"No RC data available!");
                }
                break;
            }
            case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW: {
                uint16_t s1, s2, s3, s4, s5, s6, s7, s8;
                if(mavlinkMap[myIndex]->getRawServos(s1, s2, s3, s4, s5, s6, s7, s8)) {
                    ppPwork_data[index][0] = (real_T) s1;
                    ppPwork_data[index][1] = (real_T) s2;
                    ppPwork_data[index][2] = (real_T) s3;
                    ppPwork_data[index][3] = (real_T) s4;
                    ppPwork_data[index][4] = (real_T) s5;
                    ppPwork_data[index][5] = (real_T) s6;
                    ppPwork_data[index][6] = (real_T) s7;
                    ppPwork_data[index][7] = (real_T) s8;
                } else {
//                    printMessage(S,"No servo data available!");
                }
                break;
            }
            case MAVLINK_MSG_ID_SYS_STATUS: {
                uint8_t mode, nav_mode, status;
                uint16_t load, vbat, battery_remaining, packet_drop;
                if(mavlinkMap[myIndex]->getSystemStatus(mode, nav_mode, status, load, vbat, battery_remaining, packet_drop)) {
                    ppPwork_data[index][0] = (real_T) mode;
                    ppPwork_data[index][1] = (real_T) nav_mode;
                    ppPwork_data[index][2] = (real_T) status;
                    ppPwork_data[index][3] = (real_T) load;
                    ppPwork_data[index][4] = (real_T) vbat;
                    ppPwork_data[index][5] = (real_T) battery_remaining;
                    ppPwork_data[index][6] = (real_T) packet_drop;
                }
            }
            
            
            default:
                break;
        }
    }
    return 1;
}
#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
