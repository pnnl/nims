//
//  tracked_object.h
//  
//
//  Created by Shari Matzner on 6/18/2015.
//  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
//

#ifndef __NIMS_TRACKED_OBJECT_HPP__
#define __NIMS_TRACKED_OBJECT_HPP__

#include <iostream>
#include <vector>
#include <cstdint>  // fixed width integer types

#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp> // kalman filter

#include "detections.h"

class TrackedObject
{
public:
    // Constructor
    //TrackedObject(const cv::Point2f& initial_pos,  cv::InputArray initial_image = cv::noArray(),
     //              int initial_epoch = 0, float process_noise = 1e-5, float measurement_noise = 1e-1);
    TrackedObject(long id, float epoch, Detection initial_det, float process_noise = 1e-5, float measurement_noise = 1e-1);
    // Modifiers
    void        update(float epoch, Detection new_det);
    Detection   predict(float epoch);
    
    // Accessors
    long get_id() { return id_; };

    const std::vector<Detection>& get_track() { return detections_; };

	//void  get_track(std::vector<long>& epoch, 
	 //               std::vector<cv::Rect>& bounding_box) const;
	//void  get_track_smoothed(std::vector<long>& epoch, 
	 //                        std::vector<cv::Point2f>& position) const;
    // get the last image of the track	
    //void  get_last_image(cv::OutputArray lastimg) const; 
    // the epoch of the last update
    float  last_epoch()   const { return epoch_.back(); };
    // the length of the track 
	long  track_length() const { return detections_.size(); }; 

private:
    void init_tracking(int Nstate, int Nmeasure, float q, float r);
    
    long                     id_; // unique identifier
    std::vector<float>        epoch_;
    std::vector<Detection>   detections_;
    cv::KalmanFilter         kf_;
    
 

};


#endif // __NIMS_TRACKED_OBJECT_HPP__
