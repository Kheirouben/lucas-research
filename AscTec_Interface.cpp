#define S_FUNCTION_NAME  AscTec_Interface
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
#include "APMMAVLink/DataLink/UDPDataLink.h"
#include "APMMAVLink/DataLink/MAVLink.h"
#include "APMMAVLink/DataLink/MAVLinkComponent.h"


#define COM_PORT_IDX 0
#define COM_PORT(S) ssGetSFcnParam(S,COM_PORT_IDX)

#define COM_BUAD_IDX 1
#define COM_BAUD(S) ssGetSFcnParam(S,COM_BUAD_IDX)

#define NPARAMS   2


#if !defined(MATLAB_MEX_FILE)
    #error This_file_can_be_used_only_during_simulation_inside_Simulink
#endif

        
        
struct CTRL_INPUT {
//serial commands (= Scientific Interface)
short pitch; //Pitch input: -2047..+2047 (0=neutral)
short roll; //Roll input: -2047..+2047 (0=neutral)
short yaw; //(=R/C Stick input) -2047..+2047 (0=neutral)
short thrust; //Collective: 0..4095 = 0..100%
short ctrl;

/*control byte:
bit 0: pitch control enabled
bit 1: roll control enabled
bit 2: yaw control enabled
bit 3: thrust control enabled
These bits can be used to only &
!enable one axis at a time and&
! thus to control the other &
!axes manually. This usually &
!helps a lot to set up and &
!finetune controllers for each&
! axis seperately. */

short chksum;
} myControl;
        
        
        
        
        
DataLink * myLink;
UDPDataLink * myUDPLink;
MAVLink * myMAVLink;
float alt;

#undef MDL_CHECK_PARAMETERS 

static void mdlInitializeSizes(SimStruct *S) {
    int_T nInputPorts  = 0;  /* number of input ports  */
    int_T nOutputPorts = 0;  /* number of output ports */

  
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

    /* Register the number and type of states the S-Function uses */
    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, 0);   /* number of discrete states             */

    if (!ssSetNumInputPorts(S, 1)) return;    
    ssSetInputPortWidth(S, 0, 1); /* Mode_ID*/
    
    if (!ssSetNumOutputPorts(S, 1)) return;
    ssSetOutputPortWidth(S, 0, 1); /* Mode_ID*/

    ssSetNumSampleTimes(S, 1);   /* number of sample times   */
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
    char_T strCOM[sizeof("COM00")];
    mxGetString(COM_PORT(S),strCOM,sizeof(strCOM));
    int baud = (int)*(real_T*) mxGetData(COM_BAUD(S));
  
    myLink = new SerialDataLink(strCOM, baud);
    myLink->connect();
    
    myUDPLink = new UDPDataLink("192.168.10.1", 4321);
    myUDPLink->connect();
    myMAVLink = new MAVLink(255,0,myUDPLink);
    myMAVLink->setTargetComponent(30,1);
    
}
#endif /*  MDL_START */


/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector(s),
 *    ssGetOutputPortSignal.
 */
static void mdlOutputs(SimStruct *S, int_T tid) {
    real_T            *y    = ssGetOutputPortRealSignal(S,0);
    real_T            *x    = ssGetRealDiscStates(S);

    y[0]=alt;
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
    InputRealPtrsType pInput = ssGetInputPortRealSignalPtrs(S,0);
    float throttle = (float) (*pInput[0]);
    myControl.pitch = 0;   
    myControl.roll = 0;   
    myControl.yaw = 0;   
    myControl.thrust = (short)(4095.0 * throttle);    
    myControl.ctrl = 0x8;
    myControl.chksum = myControl.pitch+ myControl.roll + myControl.yaw + myControl.thrust + myControl.ctrl + 0xAAAA;

    char *ptr = (char*)&myControl;
    
    char buffer[128];
    sprintf(buffer,">*>di");
    myLink->send(buffer, 5);
    Sleep(1);
    myLink->send(ptr, 12);
    
    myMAVLink->receiveMessage();
    float t1,t2,t5;
    int16_t t3;
    uint16_t t4;
    float tmp_alt;
    myMAVLink->getVFRHUD(t1,t2,t3,t4,tmp_alt,t5);
    if (tmp_alt > 0) {
        alt = tmp_alt;
    }

}
#endif /* MDL_UPDATE */
static void mdlTerminate(SimStruct *S) {
    myLink->disconnect();
    myUDPLink->disconnect();
    myLink = NULL;
    myUDPLink = NULL;
    myMAVLink = NULL;
}


#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
