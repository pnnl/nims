/*
    This file has been generated by bvtidl.pl. DO NOT MODIFY!
*/

#ifndef __CPP_BVTEVENTMARK_HPP__
#define __CPP_BVTEVENTMARK_HPP__

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

namespace BVTSDK
{

    /// <summary>
    /// An EventMark allows the user to insert arbitrary information into a sonar file.  The user can
    /// store two strings, one called a "Key" string and the other a "Text" string, in an EventMark. 
    /// The EventMarks are stored and retrieved independently of Heads or Pings.
    /// NOTE: EventMark objects will only be saved to a sonar of type FILE. 
    /// <summary>
    class EventMark
    {
    public:
        EventMark()
        {
            BVTEventMark p = BVTEventMark_Create();
            _owned = std::shared_ptr<BVTOpaqueEventMark>(p, BVTEventMark_Destroy);
        }
    public: /*consider private*/
        EventMark(BVTEventMark p)
        {
            _owned = std::shared_ptr<BVTOpaqueEventMark>(p, BVTEventMark_Destroy);
        }
    public:
        ~EventMark()
        {
            
        }

    //public:
    //  EventMark(const EventMark &) {}
    //  EventMark& operator=(const EventMark &) {}
    public:
        /// SDK object pointer
        BVTEventMark Handle() const
        {
            return _owned.get();
        }
    private:
        std::shared_ptr<BVTOpaqueEventMark> _owned;

    public:
        /// <summary>
        /// The max length of an EventMark key string 
        /// </summary>
        static const int MAX_KEYLENGTH = 80;

        /// <summary>
        /// The max length of an EventMark text string 
        /// </summary>
        static const int MAX_TEXTLENGTH = 512;

        //
        // Return the timestamp in seconds since 00:00:00 UTC, January 1, 1970 
        // The timestamp is a standard UNIX time stamp. This is 
        // a similar value to that returned by the time() C standard library 
        // function. In fact, the only difference is the addition of fractional seconds.
        //
        // @param timestamp timestamp of event mark         
        double GetTimestamp()
        {
            double timestamp;
            int error_code = BVTEventMark_GetTimestamp(_owned.get(), /* out */ &timestamp);
            return timestamp;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the number of characters stored in the Key string, not including the null-term character.  You'll need to allocate a
        // character buffer of at least length @ref GetKeyStringLength()+1 to retrieve the entire Key.
        // You can also allocate a buffer of @ref MAX_KEYLENGTH at compile time.
        //
        // @param length key length         
        int GetKeyStringLength()
        {
            int length;
            int error_code = BVTEventMark_GetKeyStringLength(_owned.get(), /* out */ &length);
            return length;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieves a copy of the EventMark's Key string.
        //
        // @param buffer buffer to hold the null-terminated string to be passed back 
        // @param buffer_size total number of characters the passed buffer can hold         
        std::string GetKeyString()
        {
            char buffer[256] = { 0 };
            int buffer_size = 255;
            int error_code = BVTEventMark_GetKeyString(_owned.get(), buffer, buffer_size);
            return std::string(buffer);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the number of characters stored in the Text string, not including the null-term character. You'll need to allocate a
        // character buffer of at least length @ref GetTextStringLength()+1 to retrieve the entire string.
        // You can also allocate a buffer of @ref MAX_TEXTLENGTH at compile time.
        //
        // @param length text string length         
        int GetTextStringLength()
        {
            int length;
            int error_code = BVTEventMark_GetTextStringLength(_owned.get(), /* out */ &length);
            return length;
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Retrieves a copy of the EventMark's Text string.
        //
        // @param buffer buffer to hold the null-terminated string to be passed back 
        // @param buffer_size total number of characters the passed buffer can hold         
        std::string GetTextString()
        {
            char buffer[256] = { 0 };
            int buffer_size = 255;
            int error_code = BVTEventMark_GetTextString(_owned.get(), buffer, buffer_size);
            return std::string(buffer);
            if (0 != error_code)
                throw SdkException(error_code);
        }

        //
        // Return the "ping-by-time" number associated with the EventMark.
        // This is typically the ping immediately before the EventMark was created.
        //
        // @param number ping number (zero-based index)     
        int GetPingNumber()
        {
            int number;
            int error_code = BVTEventMark_GetPingNumber(_owned.get(), /* out */ &number);
            return number;
            if (0 != error_code)
                throw SdkException(error_code);
        }

    };

}

#endif