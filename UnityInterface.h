#pragma once

//-----------------------------------------------------------------------------
// Headers
//-----------------------------------------------------------------------------

#include "stdint.h"
#include <NiTE.h>


//-----------------------------------------------------------------------------
// Interface types
//-----------------------------------------------------------------------------

//#ifdef UNITYINTERFACE_EXPORTS
#define UNITYINTERFACE_API extern "C"
//#else
//#define UNITYINTERFACE_API extern "C" __declspec(dllimport)
//#endif

typedef void (__stdcall * pfn_user_callback)(int nUserId);
//typedef void (__stdcall * pfn_focus_callback)(bool focused);
//typedef void (__stdcall * pfn_handpoint_callback)(float x, float y, float z);
//typedef void (__stdcall * pfn_item_callback)(int item_index, int direction);
//typedef void (__stdcall * pfn_value_callback)(float value);

//-----------------------------------------------------------------------------
// UnityInterface API
//-----------------------------------------------------------------------------

// Device enumeration
UNITYINTERFACE_API openni::Status GetDeviceCount(int *pCount);
//UNITYINTERFACE_API const char * GetDeviceNameByIndex(int index);

// Init update & shutdown
UNITYINTERFACE_API openni::Status Init(const bool isInitDepthStream, const bool isInitColorStream, const bool isInitInfraredStream);
UNITYINTERFACE_API void Shutdown();
UNITYINTERFACE_API openni::Status Update(nite::UserId *pUsers, int16_t *pStates, int32_t *pUsersCount);
UNITYINTERFACE_API const char *	GetLastErrorString();

// Getters - only call after Init
UNITYINTERFACE_API unsigned int	GetDepthWidth();
UNITYINTERFACE_API unsigned int	GetDepthHeight();
UNITYINTERFACE_API unsigned int	GetInfraredWidth();
UNITYINTERFACE_API unsigned int	GetInfraredHeight();
UNITYINTERFACE_API unsigned int	GetColorWidth();
UNITYINTERFACE_API unsigned int	GetColorHeight();
UNITYINTERFACE_API const nite::UserId* GetUsersLabelMap();
UNITYINTERFACE_API const openni::DepthPixel* GetUsersDepthMap();
UNITYINTERFACE_API const openni::Grayscale16Pixel* GetUsersInfraredMap();
UNITYINTERFACE_API const openni::RGB888Pixel* GetUsersColorMap();

// User tracking
UNITYINTERFACE_API void	StartLookingForUsers(pfn_user_callback pNewUser, pfn_user_callback pCalibrationStarted, 
                                             pfn_user_callback pCalibrationFailed, pfn_user_callback pCalibrationSuccess, 
                                             pfn_user_callback pUserLost);
UNITYINTERFACE_API void	StopLookingForUsers();
//UNITYINTERFACE_API void	LoseUsers();
//UNITYINTERFACE_API bool	GetUserCenterOfMass(XnUserID userID, OUT XnVector3D * pCom);
//UNITYINTERFACE_API float GetUserPausePoseProgress(nite::UserId userID);

// Skeleton stuff
UNITYINTERFACE_API void SetSkeletonSmoothing(float f);
UNITYINTERFACE_API bool GetJointTransformation(int userID, nite::JointType joint, nite::SkeletonJoint* pTransformation);
UNITYINTERFACE_API bool GetJointPosition(int userID, nite::JointType joint, nite::Point3f* pPosition);
UNITYINTERFACE_API bool GetJointOrientation(int userID, nite::JointType joint, nite::Quaternion* pOrientation);
UNITYINTERFACE_API float GetJointPositionConfidence(int userID, nite::JointType joint);
UNITYINTERFACE_API float GetJointOrientationConfidence(int userID, nite::JointType joint);

// Coordinate conversion
UNITYINTERFACE_API bool ConvertDepthToColor(int depthX, int depthY, openni::DepthPixel depthZ, int *pColorX, int *pColorY);
UNITYINTERFACE_API bool ConvertDepthToWorld(float depthX, float depthY, float depthZ, float *pWorldX, float *pWorldY, float *pWorldZ);
UNITYINTERFACE_API bool ConvertWorldToDepth(float worldX, float worldY, float worldZ, int *pDepthX, int *pDepthY, openni::DepthPixel *pDepthZ);

