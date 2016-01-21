#include "UnityInterface.h"
#include <OpenNI.h>
//#include "UsersManager.h"


#define MAX_USERS 10


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// OpenNI stuff
static openni::Device		        m_device;
static openni::VideoStream	        g_DepthStream;
static openni::VideoStream	        g_InfraredStream;
static openni::VideoStream	        g_ColorStream;
static openni::CoordinateConverter  g_CoordConverter;

static unsigned int			        g_nDepthWidth	= 0;
static unsigned int			        g_nDepthHeight	= 0;
static openni::VideoFrameRef        g_frameDepth;

static unsigned int			        g_nInfraredWidth	= 0;
static unsigned int			        g_nInfraredHeight	= 0;
static openni::VideoFrameRef        g_frameInfrared;

static unsigned int			        g_nColorWidth	= 0;
static unsigned int			        g_nColorHeight	= 0;
static openni::VideoFrameRef        g_frameColor;

static nite::UserTracker*	        g_pUserTracker = NULL;
static nite::UserTrackerFrameRef    g_userTrackerFrame;
static nite::SkeletonState          g_skeletonStates[100] = {nite::SKELETON_NONE};
static bool					        g_bSearchingForPlayers = false;


//-----------------------------------------------------------------------------
// Callbacks
//-----------------------------------------------------------------------------

static pfn_user_callback g_pfnNewUser = NULL;
static pfn_user_callback g_pfnCalibrationStarted = NULL;
static pfn_user_callback g_pfnCalibrationSucceeded = NULL;
static pfn_user_callback g_pfnCalibrationFailed = NULL;
static pfn_user_callback g_pfnTrackingStopped = NULL;
static pfn_user_callback g_pfnUserLost = NULL;


//-----------------------------------------------------------------------------
// UnityInterface API
//-----------------------------------------------------------------------------

openni::Status GetDeviceCount(int *pCount)
{
    openni::Status rc = openni::STATUS_OK;
    
    rc = openni::OpenNI::initialize();
    if (rc != openni::STATUS_OK)
    {
        return rc;
    }
    
    openni::Array<openni::DeviceInfo> deviceList;
    openni::OpenNI::enumerateDevices(&deviceList);
    
    *pCount = deviceList.getSize();
    openni::OpenNI::shutdown();
    
    return rc;
}

//const char * GetDeviceNameByIndex(int index)
//{
//    openni::Status rc = openni::STATUS_OK;
//    const char* deviceUri = openni::ANY_DEVICE;
//
//	rc = openni::OpenNI::initialize();
//	if (rc != openni::STATUS_OK)
//	{
//		return deviceUri;
//	}
//
//	openni::Array<openni::DeviceInfo> deviceList;
//	openni::OpenNI::enumerateDevices(&deviceList);
//
//    if(index >= 0 && index < deviceList.getSize())
//    {
//        deviceUri = deviceList[index].getUri();
//    }
//
//    openni::OpenNI::shutdown();
//
//    return deviceUri;
//}

openni::Status Init(const bool isInitDepthStream, const bool isInitColorStream, const bool isInitInfraredStream)
{
    // Init NITE context
    openni::Status rc = openni::OpenNI::initialize();
    if (rc != openni::STATUS_OK)
    {
        Shutdown();
        return rc;
    }
    
    // Save width & height of our depth generator (also ensures we will have one)
    const char* deviceUri = openni::ANY_DEVICE;
    rc = m_device.open(deviceUri);
    if (rc != openni::STATUS_OK)
    {
        return rc;
    }
    
    nite::NiTE::initialize();
    
    if (isInitDepthStream && m_device.getSensorInfo(openni::SENSOR_DEPTH) != NULL)
    {
        rc = g_DepthStream.create(m_device, openni::SENSOR_DEPTH);
        if (rc == openni::STATUS_OK)
        {
            g_nDepthWidth = g_DepthStream.getVideoMode().getResolutionX();
            g_nDepthHeight = g_DepthStream.getVideoMode().getResolutionY();
            
            //g_DepthStream.setMirroringEnabled(true);
            
            rc = g_DepthStream.start();
            if (rc != openni::STATUS_OK)
            {
                Shutdown();
                return rc;
            }
        }
        else
        {
            Shutdown();
            return rc;
        }
    }
    
    if (isInitInfraredStream && m_device.getSensorInfo(openni::SENSOR_IR) != NULL)
    {
        rc = g_InfraredStream.create(m_device, openni::SENSOR_IR);
        if (rc == openni::STATUS_OK)
        {
            g_nInfraredWidth = g_InfraredStream.getVideoMode().getResolutionX();
            g_nInfraredHeight = g_InfraredStream.getVideoMode().getResolutionY();
            
            //g_InfraredStream.setMirroringEnabled(true);
            
            rc = g_InfraredStream.start();
            if (rc != openni::STATUS_OK)
            {
                Shutdown();
                return rc;
            }
        }
        else
        {
            Shutdown();
            return rc;
        }
    }
    
    if (isInitColorStream && m_device.getSensorInfo(openni::SENSOR_COLOR) != NULL)
    {
        rc = g_ColorStream.create(m_device, openni::SENSOR_COLOR);
        if (rc == openni::STATUS_OK)
        {
            g_nColorWidth = g_ColorStream.getVideoMode().getResolutionX();
            g_nColorHeight = g_ColorStream.getVideoMode().getResolutionY();
            
            //g_ColorStream.setMirroringEnabled(true);
            
            rc = g_ColorStream.start();
            if (rc != openni::STATUS_OK)
            {
                Shutdown();
                return rc;
            }
        }
        else
        {
            Shutdown();
            return rc;
        }
    }
    
    // get the user tracker
    g_pUserTracker = new nite::UserTracker;
    nite::Status rcNite = g_pUserTracker->create(&m_device);
    if (rcNite != nite::STATUS_OK)
    {
        Shutdown();
        return openni::STATUS_ERROR;
    }
    
    g_pUserTracker->setSkeletonSmoothingFactor(0.5);
    
    return openni::STATUS_OK;
}

void Shutdown()
{
    g_userTrackerFrame.release();
    if(g_pUserTracker != NULL)
    {
        delete g_pUserTracker;
        g_pUserTracker = NULL;
    }
    
    g_frameDepth.release();
    g_frameInfrared.release();
    g_frameColor.release();
    
    g_DepthStream.stop();
    g_InfraredStream.stop();
    g_ColorStream.stop();
    
    g_DepthStream.destroy();
    g_InfraredStream.destroy();
    g_ColorStream.destroy();
    
    m_device.close();
    
    // Bye bye open ni
    nite::NiTE::shutdown();
    openni::OpenNI::shutdown();
}

//-----------------------------------------------------------------------------
// Internal event handlers
//-----------------------------------------------------------------------------

static void OnNewUser(const nite::UserId nUserId)
{
    if (g_bSearchingForPlayers)
    {
        if (NULL != g_pfnNewUser)
        {
            g_pfnNewUser(nUserId);
        }
    }
}

static void OnCalibrationStarted(const nite::UserId nUserId)
{
    if (NULL != g_pfnCalibrationStarted)
    {
        g_pfnCalibrationStarted(nUserId);
    }
}

static void OnCalibrationSucceeded(const nite::UserId nUserId)
{
    if (NULL != g_pfnCalibrationSucceeded)
    {
        g_pfnCalibrationSucceeded(nUserId);
    }
}

static void OnCalibrationFailed(const nite::UserId nUserId)
{
    if (NULL != g_pfnCalibrationFailed)
    {
        g_pfnCalibrationFailed(nUserId);
    }
}

static void OnTrackingStopped(const nite::UserId nUserId)
{
    if (NULL != g_pfnTrackingStopped)
    {
        g_pfnTrackingStopped(nUserId);
    }
}

static void OnUserLost(const nite::UserId nUserId)
{
    if (NULL != g_pfnUserLost)
    {
        g_pfnUserLost(nUserId);
    }
}

//-----------------------------------------------------------------------------

openni::Status Update(nite::UserId *pUsers, int16_t *pStates, int32_t *pUsersCount)
{
    int32_t maxUsersLen = *pUsersCount;
    *pUsersCount = 0;
    
    if(g_DepthStream.isValid() || g_InfraredStream.isValid() || g_ColorStream.isValid())
    {
        openni::VideoStream* streams[] = {&g_DepthStream, &g_InfraredStream, &g_ColorStream};
        int readyStream = -1;
        
        openni::Status rc = openni::OpenNI::waitForAnyStream(streams, 3, &readyStream);
        if (rc != openni::STATUS_OK)
        {
            return rc;
        }
        
        switch (readyStream)
        {
            case 0:
                // Depth
                g_DepthStream.readFrame(&g_frameDepth);
                break;
                
            case 1:
                // Infrared
                g_InfraredStream.readFrame(&g_frameInfrared);
                break;
                
            case 2:
                // Color
                g_ColorStream.readFrame(&g_frameColor);
                break;
        }
    }
    
    nite::Status niteRc = g_pUserTracker->readFrame(&g_userTrackerFrame);
    if (niteRc != nite::STATUS_OK)
    {
        return openni::STATUS_ERROR;
    }
    
    const nite::Array<nite::UserData>& users = g_userTrackerFrame.getUsers();
    for (int i = 0; i < users.getSize(); ++i)
    {
        const nite::UserData& user = users[i];
        
        if (user.isNew())
        {
            g_skeletonStates[user.getId()] = (nite::SkeletonState)0;
            
            if((*pUsersCount) < maxUsersLen)
            {
                pUsers[*pUsersCount] = user.getId();
                pStates[*pUsersCount] = 1;  // New User
                (*pUsersCount)++;
            }
            
            OnNewUser(user.getId());
            g_pUserTracker->startSkeletonTracking(user.getId());
            //g_pUserTracker->startPoseDetection(user.getId(), nite::POSE_PSI);
        }
        else if (user.isLost())
        {
            g_skeletonStates[user.getId()] = (nite::SkeletonState)-1;
            
            if((*pUsersCount) < maxUsersLen)
            {
                pUsers[*pUsersCount] = user.getId();
                pStates[*pUsersCount] = 5;  // User Lost
                (*pUsersCount)++;
            }
            
            OnUserLost(user.getId());
        }
        
        if(g_skeletonStates[user.getId()] != user.getSkeleton().getState())
        {
            switch(g_skeletonStates[user.getId()] = user.getSkeleton().getState())
            {
                case nite::SKELETON_NONE:
                    if((*pUsersCount) < maxUsersLen)
                    {
                        pUsers[*pUsersCount] = user.getId();
                        pStates[*pUsersCount] = 6;  // Tracking stopped
                        (*pUsersCount)++;
                    }
                    
                    OnTrackingStopped(user.getId());
                    break;
                    
                case nite::SKELETON_CALIBRATING:
                    if((*pUsersCount) < maxUsersLen)
                    {
                        pUsers[*pUsersCount] = user.getId();
                        pStates[*pUsersCount] = 2;  // Calibration started
                        (*pUsersCount)++;
                    }
                    
                    OnCalibrationStarted(user.getId());
                    break;
                    
                case nite::SKELETON_TRACKED:
                    if((*pUsersCount) < maxUsersLen)
                    {
                        pUsers[*pUsersCount] = user.getId();
                        pStates[*pUsersCount] = 3;  // Calibration succeeded
                        (*pUsersCount)++;
                    }
                    
                    OnCalibrationSucceeded(user.getId());
                    break;
                    
                case nite::SKELETON_CALIBRATION_ERROR_NOT_IN_POSE:
                case nite::SKELETON_CALIBRATION_ERROR_HANDS:
                case nite::SKELETON_CALIBRATION_ERROR_LEGS:
                case nite::SKELETON_CALIBRATION_ERROR_HEAD:
                case nite::SKELETON_CALIBRATION_ERROR_TORSO:
                    if((*pUsersCount) < maxUsersLen)
                    {
                        pUsers[*pUsersCount] = user.getId();
                        pStates[*pUsersCount] = 4;  // Calibration failed
                        (*pUsersCount)++;
                    }
                    
                    OnCalibrationFailed(user.getId());
                    break;
            }
        }
    }
    
    return openni::STATUS_OK;
}

const char * GetLastErrorString()
{
    return openni::OpenNI::getExtendedError();
}

unsigned int GetDepthWidth()
{
    return g_nDepthWidth;
}

unsigned int GetDepthHeight()
{
    return g_nDepthHeight;
}

unsigned int GetInfraredWidth()
{
    return g_nInfraredWidth;
}

unsigned int GetInfraredHeight()
{
    return g_nInfraredHeight;
}

unsigned int GetColorWidth()
{
    return g_nColorWidth;
}

unsigned int GetColorHeight()
{
    return g_nColorHeight;
}

void StartLookingForUsers(pfn_user_callback pNewUser, pfn_user_callback pCalibrationStarted,
                          pfn_user_callback pCalibrationFailed, pfn_user_callback pCalibrationSuccess,
                          pfn_user_callback pUserLost)
{
    // save our callbacks
    g_pfnNewUser = pNewUser;
    g_pfnCalibrationStarted = pCalibrationStarted;
    g_pfnCalibrationFailed = pCalibrationFailed;
    g_pfnCalibrationSucceeded = pCalibrationSuccess;
    //g_pfnTrackingStopped = pTrackingStopped;
    g_pfnUserLost = pUserLost;
    
    // make sure we try to calibrate future players
    g_bSearchingForPlayers = true;
}

void StopLookingForUsers()
{
    g_bSearchingForPlayers = false;
}

const nite::UserId* GetUsersLabelMap()
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserMap& userLabels = g_userTrackerFrame.getUserMap();
        return userLabels.getPixels();
    }
    
    return NULL;
}

const openni::DepthPixel* GetUsersDepthMap()
{
    if(g_frameDepth.isValid())
        return (openni::DepthPixel*)g_frameDepth.getData();
    else
        return NULL;
}

const openni::Grayscale16Pixel* GetUsersInfraredMap()
{
    if(g_frameInfrared.isValid())
        return (openni::Grayscale16Pixel*)g_frameInfrared.getData();
    else
        return NULL;
}

const openni::RGB888Pixel* GetUsersColorMap()
{
    if(g_frameColor.isValid())
        return (openni::RGB888Pixel*)g_frameColor.getData();
    else
        return NULL;
}

void SetSkeletonSmoothing(float f)
{
    if(g_pUserTracker)
    {
        g_pUserTracker->setSkeletonSmoothingFactor(f);
    }
}

bool GetJointTransformation(int userID, nite::JointType joint, nite::SkeletonJoint* pTransformation)
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserData* pUser = g_userTrackerFrame.getUserById(userID);
        
        if(pUser && (pUser->getId() == userID) && (pUser->getSkeleton().getState() == nite::SKELETON_TRACKED))
        {
            (*pTransformation) = pUser->getSkeleton().getJoint(joint);
            return true;
        }
    }
    
    return false;
    
}

bool GetJointPosition(int userID, nite::JointType joint, nite::Point3f* pPosition)
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserData* pUser = g_userTrackerFrame.getUserById(userID);
        const nite::SkeletonJoint skelJoint = pUser->getSkeleton().getJoint(joint);
        
        if(pUser && (pUser->getId() == userID) && (pUser->getSkeleton().getState() == nite::SKELETON_TRACKED) &&
           (skelJoint.getPositionConfidence() > 0.5))
        {
            (*pPosition) = skelJoint.getPosition();
            return true;
        }
    }
    
    return false;
}

bool GetJointOrientation(int userID, nite::JointType joint, nite::Quaternion* pOrientation)
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserData* pUser = g_userTrackerFrame.getUserById(userID);
        const nite::SkeletonJoint skelJoint = pUser->getSkeleton().getJoint(joint);
        
        if(pUser && (pUser->getId() == userID) && (pUser->getSkeleton().getState() == nite::SKELETON_TRACKED) &&
           (skelJoint.getOrientationConfidence() > 0.5))
        {
            (*pOrientation) = skelJoint.getOrientation();
            return true;
        }
    }
    
    return false;
}

float GetJointPositionConfidence(int userID, nite::JointType joint)
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserData* pUser = g_userTrackerFrame.getUserById(userID);
        
        if(pUser && (pUser->getId() == userID) && (pUser->getSkeleton().getState() == nite::SKELETON_TRACKED))
        {
            return pUser->getSkeleton().getJoint(joint).getPositionConfidence();
        }
    }
    
    return 0.0f;
}

float GetJointOrientationConfidence(int userID, nite::JointType joint)
{
    if(g_userTrackerFrame.isValid())
    {
        const nite::UserData* pUser = g_userTrackerFrame.getUserById(userID);
        
        if(pUser && (pUser->getId() == userID) && (pUser->getSkeleton().getState() == nite::SKELETON_TRACKED))
        {
            return pUser->getSkeleton().getJoint(joint).getOrientationConfidence();
        }
    }
    
    return 0.0f;
}

bool ConvertDepthToColor(int depthX, int depthY, openni::DepthPixel depthZ, int *pColorX, int *pColorY)
{
    int rc = g_CoordConverter.convertDepthToColor(g_DepthStream, g_ColorStream, depthX, depthY, depthZ, pColorX, pColorY);
    return (rc == openni::STATUS_OK);
}

bool ConvertDepthToWorld(float depthX, float depthY, float depthZ, float *pWorldX, float *pWorldY, float *pWorldZ)
{
    openni::Status rc = g_CoordConverter.convertDepthToWorld(g_DepthStream, depthX, depthY, depthZ, pWorldX, pWorldY, pWorldZ);
    return (rc == openni::STATUS_OK);
}

bool ConvertWorldToDepth(float worldX, float worldY, float worldZ, int *pDepthX, int *pDepthY, openni::DepthPixel *pDepthZ)
{
    openni::Status rc = g_CoordConverter.convertWorldToDepth(g_DepthStream, worldX, worldY, worldZ, pDepthX, pDepthY, pDepthZ);
    return (rc == openni::STATUS_OK);
}

