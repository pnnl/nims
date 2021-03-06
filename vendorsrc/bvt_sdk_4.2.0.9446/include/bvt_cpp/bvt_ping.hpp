/*
    This file has been generated by bvtidl.pl. DO NOT MODIFY!
*/

#ifndef __CPP_BVTPING_HPP__
#define __CPP_BVTPING_HPP__

#include <string>
#ifdef _WIN32
#   include <memory>
#else
#   include <cstddef>
#   if defined (__GLIBCXX__) && __cplusplus <= 199711L
#      include <tr1/memory>
       namespace std {
          using std::tr1::shared_ptr;
       }
#   else
#       include <memory>
#   endif
#endif
#include <bvt_cpp/bvt_retval.hpp>
#include <bvt_cpp/bvt_error.hpp>
#include <bvt_cpp/bvt_magimage.hpp>
#include <bvt_cpp/bvt_navdata.hpp>
#include <bvt_cpp/bvt_orientation.hpp>

namespace BVTSDK
{

    class Orientation;
    class NavData;
    class MagImage;

    /// <summary>
    /// As its name implies, the Ping object represents the return from 
    /// a single ping on a particular head. <br>
    /// A ping is essentially a container for data. As such, after you get
    /// a ping from the head and extract the data (or save it to a file),
    /// it is necessary to destroy the ping object to free up memory. <br>
    /// Each ping may also store navigation data to indicate the position
    /// and orientation of the vehicle at the time of the ping. <br>
    /// Each ping may have a video frame associated with it, and saved in
    /// the same file. These images are typically from a video camera
    /// mounted near the sonar, such as on a ROV. <br>
    /// <summary>
    class Ping
    {
    public:
    public: /*consider private*/
        Ping(BVTPing p)
        {
            _owned = std::shared_ptr<BVTOpaquePing>(p, BVTPing_Destroy);
        }
    public:
        ~Ping()
        {
            
        }

    //public:
    //  Ping(const Ping &) {}
    //  Ping& operator=(const Ping &) {}
    public:
        /// SDK object pointer
        BVTPing Handle() const
        {
            return _owned.get();
        }
    private:
        std::shared_ptr<BVTOpaquePing> _owned;

    //  Head Head() const
    //  {
    //      return *_parent;
    //  }
    // private:
    //  Head *_parent;

    public:
        /// <summary>
        /// Video frame is raw RGB (RGBRGB...) 
        /// </summary>
        static const int VIDEO_RGB = 0;

        /// <summary>
        /// Video frame is a JPEG image 
        /// </summary>
        static const int VIDEO_JPEG = 1;

        //
        // Creates and returns a copy of this ping.
        // \attention Don't forget to call Destroy on the returned copy.
        //
        // @param the_copy A copy of this ping.         
        Ping Copy()
        {
            BVTPing the_copy_ptr = NULL;
            int error_code = BVTPing_Copy(_owned.get(), & the_copy_ptr);
            return Ping(the_copy_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Change the range window of this ping by removing data from the start or the end of the received singnal.
        // \attention This will fail if you try to decrease the existing start range or increase the existing stop range
        //
        // @param new_start The new start range in meters 
        // @param new_stop The new stop range in meters         
        void AdjustRange(float new_start, float new_stop)
        {
            int error_code = BVTPing_AdjustRange(_owned.get(), new_start, new_stop);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the ping number (i.e., index in file).
        // Ping numbers only have meaning if the ping came from a file.
        //
        // @param number ping number        
        int GetPingNumber()
        {
            int number;
            int error_code = BVTPing_GetPingNumber(_owned.get(), /* out */ &number);
            return number;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the head ID this ping was captured, or saved with.
        //
        // @param id head id        
        int GetHeadID()
        {
            int id;
            int error_code = BVTPing_GetHeadID(_owned.get(), /* out */ &id);
            return id;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the ping's timestamp in seconds since 00:00:00 UTC, January 1, 1970 
        // Pings are timestamped using a standard UNIX time stamp. This is 
        // a similar value to that returned by the time() C standard library 
        // function. In fact, the only difference is the addition of fractional seconds.
        //
        // @param timestamp ping timestamp      
        double GetTimestamp()
        {
            double timestamp;
            int error_code = BVTPing_GetTimestamp(_owned.get(), /* out */ &timestamp);
            return timestamp;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the ping's timestamp's offset in seconds from UTC time.  Add this value to that returned by GetTimestamp() to obtain UTC time.
        //
        // @param offset timezone offset        
        int GetTimeZoneOffset()
        {
            int offset;
            int error_code = BVTPing_GetTimeZoneOffset(_owned.get(), /* out */ &offset);
            return offset;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Set the ping's internal time stamp.  See GetTimestamp() for more information. 
        // Note: BlueView strongly recommends that users NOT directly set the time stamp as 
        // it is set internally when the ping is actually initiated.  If you are trying to 
        // synchronize two systems, it is far better to simply make sure that the system
        // clocks are synchronized, as the ping timestamp is created from the 
        // PC's internal clock. Network Time Protocol and GPS sources provide highly 
        // accurate ways to accomplish this.
        // Caution: This also overwrites the ping's recorded time zone offset with the client operating system's time zone offset.
        // Call SetTimeZoneOffset after SetTimestamp if you need to maintain the original offset.
        //
        // @param sec Timestamp in seconds since 00:00:00 UTC, January 1, 1970      
        void SetTimestamp(double sec)
        {
            int error_code = BVTPing_SetTimestamp(_owned.get(), sec);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Set the ping's time zone offset in seconds from UTC time.
        //
        // @param timeZoneOffset The timestamp's offset in seconds from UTC time.       
        void SetTimeZoneOffset(int timeZoneOffset)
        {
            int error_code = BVTPing_SetTimeZoneOffset(_owned.get(), timeZoneOffset);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieve the angular limits of the ping's field-of-view. Note that the limits
        // reported represent only the minimum and maximum angles associated with any
        // pixel in the FOV. It is possible (depending on sonar configuration) that not
        // all pixels within the reported field-of-view are populated with data.
        //
        // @param minAngleInDegrees minimum angle in the ping's field-of-view
        // @param maxAngleInDegrees maximum angle in the ping's field-of-view       
        void GetFOV(float *minAngleInDegrees, float *maxAngleInDegrees)
        {
            int error_code = BVTPing_GetFOV(_owned.get(), minAngleInDegrees, maxAngleInDegrees);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the ping's the start range in meters.
        //
        // @param range This ping's start range in meters       
        float GetStartRange()
        {
            float range;
            int error_code = BVTPing_GetStartRange(_owned.get(), /* out */ &range);
            return range;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the stop range in meters.
        //
        // @param range This ping's stop range in meters        
        float GetStopRange()
        {
            float range;
            int error_code = BVTPing_GetStopRange(_owned.get(), /* out */ &range);
            return range;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the sound speed this ping was recorded at
        //
        // @param sound_speed the current speed of sound in water for this ping in meters per second        
        int GetSoundSpeed()
        {
            int sound_speed;
            int error_code = BVTPing_GetSoundSpeed(_owned.get(), /* out */ &sound_speed);
            return sound_speed;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieve an image of this ping, according to the parameters set
        // in the head used to get this ping. See Head and MagImage documentation
        // for more details.
        //
        // @param img image         
        MagImage GetImage()
        {
            BVTMagImage img_ptr = NULL;
            int error_code = BVTPing_GetImage(_owned.get(), & img_ptr);
                        return MagImage(img_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieve an XY-format image of this ping, according to the parameters set
        // in the head used to get this ping. Use BVTHead_SetImageSizeXY() to set the size for this image.
        // See Head and MagImage documentation for more details.
        //
        // @param img image         
        MagImage GetImageXY()
        {
            BVTMagImage img_ptr = NULL;
            int error_code = BVTPing_GetImageXY(_owned.get(), & img_ptr);
                        return MagImage(img_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieve an RTheta-format image of this ping, according to the parameters set
        // in the head used to get this ping. Use BVTHead_SetImageWidthRTheta() to set the size for this image.
        // See Head and MagImage documentation for more details.
        //
        // @param img image         
        MagImage GetImageRTheta()
        {
            BVTMagImage img_ptr = NULL;
            int error_code = BVTPing_GetImageRTheta(_owned.get(), & img_ptr);
                        return MagImage(img_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Returns the percentage of dyn. range (0.0 - 1.0) of the max value in this
        // ping. A value of 1.0 indicates clipped data. A value near 0.0 indicates 
        // that no targets were illuminated.
        //
        // @param max max return signal         
        float GetMaxReturnSignal()
        {
            float max;
            int error_code = BVTPing_GetMaxReturnSignal(_owned.get(), /* out */ &max);
            return max;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Creates a copy of the navigation data stored with this ping. Note
        // that the data is copied out of the ping into the local NavData object,
        // a pointer to internal data is not returned. Thus, the NavData object
        // may be used after the Ping is destroyed.
        //
        // @param nav_data A copy of the NavData stored in this object.         
        NavData GetNavDataCopy()
        {
            BVTNavData nav_data_ptr = NULL;
            int error_code = BVTPing_GetNavDataCopy(_owned.get(), & nav_data_ptr);
                        return NavData(nav_data_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Creates a copy of the navigation data stored with this ping. Note
        // that the data is copied out of the ping into the local NavData object,
        // a pointer to internal data is not returned. Thus, the NavData object
        // may be used after the Ping is destroyed.
        //
        // @param nav_data A copy of the NavData stored in this object.         
        bool TryGetNavDataCopy(NavData *nav_data)
        {
            BVTNavData nav_data_ptr = NULL;
            int error_code = BVTPing_GetNavDataCopy(_owned.get(), & nav_data_ptr);
            *nav_data = NavData(nav_data_ptr);
            return true;
            return true;
            if (0 != error_code)
            {
                *nav_data = NULL;
                return false;
            }
        }

        //
        // Returns true if this ping has a valid Navigation data object.
        //
        // @param has_nav_data True if this ping has a valid Navigation data object         
        bool HasNavData()
        {
            int has_nav_data;
            int error_code = BVTPing_HasNavData(_owned.get(), /* out */ &has_nav_data);
            return has_nav_data > 0;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Stores a copy of the navigation data with the other ping data, so the data
        // will be saved if the ping is saved to a file.
        //
        // @param nav_data Navigation data object to copy into this Ping        
        void SetNavData(const NavData & nav_data)
        {
            int error_code = BVTPing_SetNavData(_owned.get(), nav_data.Handle());
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Returns the video frame associated with this ping.
        // The video frame may be in any of the supported image formats.
        // Some image formats may already contain parameters such as height
        // and width (and more), but valid pointers must be passed in anyway.
        // The same pointer can be passed in for multiple parameters, if
        // those parameters will not be used. However, they are provided both
        // for formats which do not have embedded size information, and so that
        // the display window may be created and/or sized without parsing
        // the image data.<br>
        // NOTE: This function will return BVT_NO_VIDEO_FRAME if there
        // is no video frame stored for the ping. <br>
        // WARNING: The data buffer must NOT be accessed after the ping object is destroyed,
        // as the pointer will no longer point to valid data and will likely crash your application!
        // So copy off the data before destroying the Ping object. <br>
        // The single value pointers must be pointers to allocated data, not just
        // pointer types. For example:<br>
        // int height, width, length, type, retval;<br>
        // int * frame_ptr;<br>
        // retval = GetVideoFrame( frame_ptr, &height, &width, &length, &type );<br>
        //
        // @param frame Pointer to a pointer to the image data to be returned 
        // @param height Pointer to return the uncompressed height of the image, in pixels 
        // @param width Pointer to return the uncompressed width of the image, in pixels 
        // @param length Pointer to return the actual size of the data buffer returned, in bytes, which may include additional metadata for some image types 
        // @param type pointer to return the type of image returned: FRAME_RGB or FRAME_JPEG        
        void GetVideoFrame(unsigned char* *frame, int *height, int *width, int *length, int *type)
        {
            int error_code = BVTPing_GetVideoFrame(_owned.get(), frame, height, width, length, type);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Store a JPEG image to save with this ping.
        // Note that the height and width values will simply be stored and
        // available to read when the frame is retrieved. These have no effect
        // on the actual image size (the image will not be resized).
        // The length however is very important, as it determines how far from
        // the passed image pointer data will be read. An incorrect length could
        // result in an application crash.
        //
        // @param frame Pointer to a single video frame
        // @param height Uncompressed height of the image, in pixels 
        // @param width Uncompressed width of the image, in pixels 
        // @param length Actual number of bytes being passed in         
        void SetVideoFrameJPEG(const unsigned char* frame, int height, int width, int length)
        {
            int error_code = BVTPing_SetVideoFrameJPEG(_owned.get(), frame, height, width, length);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Stores a copy of the per ping Positioner Orientation data in the ping, so the data
        // will be saved if the ping is saved to a file. This orientation object represents
        // the dynamic position of the head as it is moved via a positioner. Other orientation
        // information will be needed to determine the absolute position of the head relative
        // to a larger coordinate system (i.e. the vessel). NOTE: the X rotation represents
        // side to side panning, and Y rotation represents vertical tilt. (which might be opposite
        // from previous SDKs; though the older files will be translated to the new convention.)
        //
        // @param orient Orientation data object to copy from       
        void SetPositionerOrientation(const Orientation & orient)
        {
            int error_code = BVTPing_SetPositionerOrientation(_owned.get(), orient.Handle());
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieves a copy of the per ping Positioner Orientation data which was stored with this ping.,
        // Note that the data is copied out of the ping into the local Orientation object,
        // a pointer to internal data is not returned. Thus, the Orientation object
        // may be used after the Ping is destroyed. (see SetPositionerOrientation, above, for more
        // information on the meaning of the PositionerOrientation data)
        //
        // @param orient Orientation data object to copy the existing Orientation data to       
        Orientation GetPositionerOrientationCopy()
        {
            BVTOrientation orient_ptr = NULL;
            int error_code = BVTPing_GetPositionerOrientationCopy(_owned.get(), & orient_ptr);
                        return Orientation(orient_ptr);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Simplified function to set just the rotations from the Ping's PositionerOrientation object.
        // The offsets and calibration values will be zero. The source and target frames will be set to
        // OR_POSITIONER and OR_HEAD, respectively.
        // This function is most useful when only pan/tilt values are available and needed.
        // See the SetPositionerOrientation() documentation for more details.
        //
        // @param X_axis_degrees pan axis rotation 
        // @param Y_axis_degrees tilt axis rotation 
        // @param Z_axis_degrees not currently used         
        void SetPositionerRotations(double X_axis_degrees, double Y_axis_degrees, double Z_axis_degrees)
        {
            int error_code = BVTPing_SetPositionerRotations(_owned.get(), X_axis_degrees, Y_axis_degrees, Z_axis_degrees);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Simplified function to get just the rotations from the Ping's PositionerOrientation object.
        //
        // @param X_axis_degrees pan axis rotation 
        // @param Y_axis_degrees tilt axis rotation 
        // @param Z_axis_degrees not currently used         
        void GetPositionerRotations(double *X_axis_degrees, double *Y_axis_degrees, double *Z_axis_degrees)
        {
            int error_code = BVTPing_GetPositionerRotations(_owned.get(), X_axis_degrees, Y_axis_degrees, Z_axis_degrees);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Returns true if this ping has a raw time series.
        //
        // @param has_signal True if this ping has a raw time series.       
        bool HasSignal()
        {
            int has_signal;
            int error_code = BVTPing_HasSignal(_owned.get(), /* out */ &has_signal);
            return has_signal > 0;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Returns true if this ping has a post-processed profile.
        //
        // @param has_stored_profile True if this ping has a post-processed profile.        
        bool HasStoredRangeProfile()
        {
            int has_stored_profile;
            int error_code = BVTPing_HasStoredRangeProfile(_owned.get(), /* out */ &has_stored_profile);
            return has_stored_profile > 0;
            if (0 != error_code)
                throw SdkException(error_code);
        }

    };

}

#endif
