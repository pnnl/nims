"""
This file was generated by bvtidl.pl.
Your changes will most likely be lost.
"""

from ctypes import *
import sys
import sdkerror


class RangeProfile(object):
    """
    @warning RangeProfile functions will fail on a sonar with old firmware, or a file recorded from a sonar with old firmware.
    The RangeProfile interface provides a 1-D view of a single ping data. It consists of a vector of ranges from the sonar head to the sonic reflection point, indexed along the the bearing (theta) dimension.
    For each angle, the range and raw intensity of the return beam at that range is stored. There are a number of approaches to choosing the reflection point. This interface provides a settable minimum intensity threshold that must be crossed and a choosable algorithm, returning either the first (nearest) point that exceeds the threshold (THRESHOLD_POLICY_NEAREST) or the distance to sample of greatest intensity (THRESHOLD_POLICY_LARGEST).
    """
    def __init__(self, handle):
        super(RangeProfile, self).__setattr__("_initialized", False)
        self._deleted = False
        if handle is None or type(handle) is not c_void_p:
            raise Exception("Class RangeProfile cannot be directly instantiated")
        else:
            self._handle = handle
        super(RangeProfile, self).__setattr__("_initialized", True)

    def __del__(self):
        self._deleted = True
        dll.BVTRangeProfile_Destroy(self._handle)

    def __setattr__(self, name, value):
        """ Don't allow setting non-existent attributes on this class
        """
        if self._initialized and not hasattr(self, name):
            raise AttributeError("%s instance has no attribute '%s'" % (self.__class__.__name__, name))
        super(RangeProfile, self).__setattr__(name, value)

    MAX_RANGE = 999
    THRESHOLD_POLICY_LARGEST = 1
    THRESHOLD_POLICY_NEAREST = 2
    @property
    def count(self):
        """
        Returns the number of range values stored for this ping.
        """
        count = c_int()
        error_code = dll.BVTRangeProfile_GetCount(self._handle, byref(count))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return count.value

    @property
    def intensity_threshold(self):
        """
        Returns the intensity threshold used to populate this RangeProfile structure. The intensity threshold serves as a noise floor, below which no sample will be considered a candidate for the beam's RangeProfile point. 
        """
        threshold = c_ushort()
        error_code = dll.BVTRangeProfile_GetIntensityThreshold(self._handle, byref(threshold))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return threshold.value

    @property
    def range_resolution(self):
        """
        Returns the resolution of the range values, in meters. The RangeProfile range value at a given bearing should be considered approximate to within +- resolution.
        """
        resolution = c_double()
        error_code = dll.BVTRangeProfile_GetRangeResolution(self._handle, byref(resolution))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return resolution.value

    @property
    def bearing_resolution(self):
        """
        Returns the resolution of the bearing (in degrees) of each RangeProfile range value. This is the difference in bearing between adjacent range values in the array.
        <br>
        """
        resolution = c_double()
        error_code = dll.BVTRangeProfile_GetBearingResolution(self._handle, byref(resolution))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return resolution.value

    @property
    def fov_min_angle(self):
        """
        Return the minimum angle for the sonar's imaging field of view.
        This is the angle of the first range value, as all
        angles are "left referenced." The angle is returned in degrees.
        Note that this may not represent the actual physical field of view
        of a particular sonar, but does represent the field of view of the
        data being returned. Some outer angles may have range values
        indicating they are out of range.
        """
        minAngle = c_float()
        error_code = dll.BVTRangeProfile_GetFOVMinAngle(self._handle, byref(minAngle))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return minAngle.value

    @property
    def fov_max_angle(self):
        """
        Return the maximum angle for the sonar's imaging field of view.
        This is the angle of the last range value, as all
        angles are "left referenced." The angle is returned in degrees.
        Note that this may not represent the actual physical field of view
        of a particular sonar, but does represent the field of view of the
        data being returned. Some outer angles may have range values
        indicating they are out of range.
        """
        maxAngle = c_float()
        error_code = dll.BVTRangeProfile_GetFOVMaxAngle(self._handle, byref(maxAngle))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return maxAngle.value

    def copy_range_values(self, ranges, number_of_ranges):
        """
        Copies the range values into the user specified buffer. The
        buffer must hold the entire number of ranges (See GetCount() above),
        or an error is returned.
        """
        error_code = dll.BVTRangeProfile_CopyRangeValues(self._handle, ranges, number_of_ranges)
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)

    def copy_bearing_values(self, bearings, number_of_bearings):
        """
        Copies the bearing values into the user specified buffer. The
        buffer must hold the entire number of bearings (See GetCount() above),
        or an error is returned.
        """
        error_code = dll.BVTRangeProfile_CopyBearingValues(self._handle, bearings, number_of_bearings)
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)

    def copy_intensity_values(self, intensities, number_of_intensities):
        """
        Copies the intensity values into the user specified buffer. The
        buffer must hold the entire number of intensities (See GetCount() above),
        or an error is returned.
        """
        error_code = dll.BVTRangeProfile_CopyIntensityValues(self._handle, intensities, number_of_intensities)
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)

    def get_range_value(self, index):
        """
        Returns the range from the sonar head, in meters, at a particular
        index into the array. <br>
        NOTE: Check all returned values for validity. If range > BVTRANGEPROFILE_MAX_RANGE
        then the range at the given bearing (index) is not valid.
        This is the result of either the nearest object at the given bearing was out of view of the sonar, or no return along that beam crossed the specified threshold.
        """
        range = c_float()
        error_code = dll.BVTRangeProfile_GetRangeValue(self._handle, index, byref(range))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return range.value

    def get_intensity_value(self, index):
        """
        Returns the intensity value at the specified index into the RangeProfile array. <br>
        """
        intensity = c_ushort()
        error_code = dll.BVTRangeProfile_GetIntensityValue(self._handle, index, byref(intensity))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return intensity.value

    def get_bearing_value(self, index):
        """
        Returns the bearing from the center of the sonar head, in degrees (positive is clockwise as viewed from above) at the given index into the RangeProfile array.
        """
        bearing = c_float()
        error_code = dll.BVTRangeProfile_GetBearingValue(self._handle, index, byref(bearing))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return bearing.value

    def get_color_image_pixel_x(self, index, image):
        """
        Returns the X coordinate for the pixel in the passed ColorImage, which
        maps to the range and bearing at the index passed. This allows placing
        of the range data on a colorimage, easing analysis of the algorithm
        used for thresholding.
        """
        x = c_int()
        error_code = dll.BVTRangeProfile_GetColorImagePixelX(self._handle, index, image._handle, byref(x))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return x.value

    def get_color_image_pixel_y(self, index, image):
        """
        Returns the Y coordinate for the pixel in the passed ColorImage which
        maps to the range and bearing at the index passed. (see similar function,
        above, for more details)
        """
        y = c_int()
        error_code = dll.BVTRangeProfile_GetColorImagePixelY(self._handle, index, image._handle, byref(y))
        if (0 != error_code):
            raise sdkerror.SDKError(error_code)
        return y.value

    class _rangesIterator:
        def __init__(self, sdk_object, func, low, high):
            self.sdk_object = sdk_object
            self.func = func
            self.current = low
            self.high = high

        def __iter__(self):
            return self

        def __len__(self):
            return self.sdk_object.count

        def next(self): # Python 3: def __next__(self)
            if self.current > self.high:
                raise StopIteration
            else:
                self.current += 1
                return self.func(self.sdk_object, self.current - 1)

    def ranges(self): # NB: ClassName.func_name Doesn't allow dependency injection
        """
        Returns an iterator calling get_range_value() from 0 to count - 1
        """
        iter = RangeProfile._rangesIterator(self, RangeProfile.get_range_value, 0, self.count - 1)
        return iter

    class _bearingsIterator:
        def __init__(self, sdk_object, func, low, high):
            self.sdk_object = sdk_object
            self.func = func
            self.current = low
            self.high = high

        def __iter__(self):
            return self

        def __len__(self):
            return self.sdk_object.count

        def next(self): # Python 3: def __next__(self)
            if self.current > self.high:
                raise StopIteration
            else:
                self.current += 1
                return self.func(self.sdk_object, self.current - 1)

    def bearings(self): # NB: ClassName.func_name Doesn't allow dependency injection
        """
        Returns an iterator calling get_bearing_value() from 0 to count - 1
        """
        iter = RangeProfile._bearingsIterator(self, RangeProfile.get_bearing_value, 0, self.count - 1)
        return iter

    class _intensitiesIterator:
        def __init__(self, sdk_object, func, low, high):
            self.sdk_object = sdk_object
            self.func = func
            self.current = low
            self.high = high

        def __iter__(self):
            return self

        def __len__(self):
            return self.sdk_object.count

        def next(self): # Python 3: def __next__(self)
            if self.current > self.high:
                raise StopIteration
            else:
                self.current += 1
                return self.func(self.sdk_object, self.current - 1)

    def intensities(self): # NB: ClassName.func_name Doesn't allow dependency injection
        """
        Returns an iterator calling get_intensity_value() from 0 to count - 1
        """
        iter = RangeProfile._intensitiesIterator(self, RangeProfile.get_intensity_value, 0, self.count - 1)
        return iter


    def get_handle(self):
        """
        SDK object pointer
        """
        return self._handle
if "win32" in sys.platform:
    dll_name = "bvtsdk4.dll"
elif "darwin" in sys.platform:
    dll_name = "libbvtsdk.dylib"
else:
    dll_name = "libbvtsdk.so"
dll = CDLL(dll_name)
dll.BVTRangeProfile_Destroy.restype = None
dll.BVTRangeProfile_Destroy.argtypes = (c_void_p,)
dll.BVTRangeProfile_GetCount.restype = c_int
dll.BVTRangeProfile_GetCount.argtypes = (c_void_p, POINTER(c_int), )
dll.BVTRangeProfile_GetIntensityThreshold.restype = c_int
dll.BVTRangeProfile_GetIntensityThreshold.argtypes = (c_void_p, POINTER(c_ushort), )
dll.BVTRangeProfile_GetRangeResolution.restype = c_int
dll.BVTRangeProfile_GetRangeResolution.argtypes = (c_void_p, POINTER(c_double), )
dll.BVTRangeProfile_GetBearingResolution.restype = c_int
dll.BVTRangeProfile_GetBearingResolution.argtypes = (c_void_p, POINTER(c_double), )
dll.BVTRangeProfile_GetFOVMinAngle.restype = c_int
dll.BVTRangeProfile_GetFOVMinAngle.argtypes = (c_void_p, POINTER(c_float), )
dll.BVTRangeProfile_GetFOVMaxAngle.restype = c_int
dll.BVTRangeProfile_GetFOVMaxAngle.argtypes = (c_void_p, POINTER(c_float), )
dll.BVTRangeProfile_CopyRangeValues.restype = c_int
dll.BVTRangeProfile_CopyRangeValues.argtypes = (c_void_p, POINTER(c_float), c_int, )
dll.BVTRangeProfile_CopyBearingValues.restype = c_int
dll.BVTRangeProfile_CopyBearingValues.argtypes = (c_void_p, POINTER(c_float), c_int, )
dll.BVTRangeProfile_CopyIntensityValues.restype = c_int
dll.BVTRangeProfile_CopyIntensityValues.argtypes = (c_void_p, POINTER(c_ushort), c_int, )
dll.BVTRangeProfile_GetRangeValue.restype = c_int
dll.BVTRangeProfile_GetRangeValue.argtypes = (c_void_p, c_int, POINTER(c_float), )
dll.BVTRangeProfile_GetIntensityValue.restype = c_int
dll.BVTRangeProfile_GetIntensityValue.argtypes = (c_void_p, c_int, POINTER(c_ushort), )
dll.BVTRangeProfile_GetBearingValue.restype = c_int
dll.BVTRangeProfile_GetBearingValue.argtypes = (c_void_p, c_int, POINTER(c_float), )
dll.BVTRangeProfile_GetColorImagePixelX.restype = c_int
dll.BVTRangeProfile_GetColorImagePixelX.argtypes = (c_void_p, c_int, c_void_p, POINTER(c_int), )
dll.BVTRangeProfile_GetColorImagePixelY.restype = c_int
dll.BVTRangeProfile_GetColorImagePixelY.argtypes = (c_void_p, c_int, c_void_p, POINTER(c_int), )

