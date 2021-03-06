#define S_FUNCTION_NAME  PC104_Interface
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

    if (!ssSetNumInputPorts(S, 2)) return;    
    ssSetInputPortWidth(S, 0, 1); /* Mode_ID*/
    ssSetInputPortWidth(S, 1, 1); /* Mode_ID*/
    
    if (!ssSetNumOutputPorts(S, 0)) return;
//    ssSetOutputPortWidth(S, 0, 1); /* Mode_ID*/

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
    myUDPLink = new UDPDataLink("131.231.105.246", 43210);
    myUDPLink->connect();
    myUDPLink->setReceiveBuffer(50);
    myMAVLink = new MAVLink(255,0,myUDPLink);
    myMAVLink->setTargetComponent(35,1);
}
#endif /*  MDL_START */


/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector(s),
 *    ssGetOutputPortSignal.
 */
static void mdlOutputs(SimStruct *S, int_T tid) {
//    real_T            *y    = ssGetOutputPortRealSignal(S,0);
//    real_T            *x    = ssGetRealDiscStates(S);

//    y[0]=alt;
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
    InputRealPtrsType pInput1 = ssGetInputPortRealSignalPtrs(S,0);
    float throttle = (float) (*pInput1[0]);
    InputRealPtrsType pInput2 = ssGetInputPortRealSignalPtrs(S,1);
    float steering = (float) (*pInput2[0]);
    myMAVLink->sendAttitudeCommand(steering, throttle, 0.0, 0.0);
    myMAVLink->sendMessages();
}
#endif /* MDL_UPDATE */

static void mdlTerminate(SimStruct *S) {
    myUDPLink->disconnect();
    myUDPLink = NULL;
    myMAVLink = NULL;
}


#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
