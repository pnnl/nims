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

struct TrackAttributes {
    uint32_t track_id;
    int first_frame; // TODO: change these to time
    int last_frame;
    float first_range;
    float last_range;
    float first_bearing;
    float last_bearing;
    float min_range;
    float max_range;
    float min_bearing;
    float max_bearing;
    int frame_count;
    int max_run;
    float mean_intensity;
    float std_intensity;
    float mean_speed;
    float std_speed;
    // TODO:  add size
};
std::ostream& operator<<(std::ostream& strm, const TrackAttributes& attr);
void print_attribute_labels(std::ostream& strm);

class TrackedObject
{
	std::vector<long>        epoch_;
	std::vector<cv::Point2f> position_;
    std::vector<cv::Mat>     image_;
    cv::KalmanFilter         kf_;
    
public:
    // Constructor
    TrackedObject(const cv::Point2f& initial_pos,  cv::InputArray initial_image = cv::noArray(),
                   int initial_epoch = 0, float process_noise = 1e-5, float measurement_noise = 1e-1);
    
    // Modifiers
    void        update(long epoch, const cv::Point2f& new_pos, cv::InputArray new_image = cv::noArray() );
    cv::Point2f predict(long epoch); // predict the position at the given timestamp
    
    // Accessors
    // get a copy of the current track
	//void  get_track(vector<long>& epoch, vector<cv::Point2f>& position) const
	                // { epoch.assign(epoch_); position.assign(position_); }; 
	void  get_track(std::vector<long>& epoch, 
	                std::vector<cv::Point2f>& position, 
	                std::vector<cv::Mat>* img) const;
	void  get_track(std::vector<long>& epoch, 
	                std::vector<cv::Rect>& bounding_box) const;
	void  get_track_smoothed(std::vector<long>& epoch, 
	                         std::vector<cv::Point2f>& position) const;
    // get the last image of the track	
    void  get_last_image(cv::OutputArray lastimg) const; 
    // the epoch of the last update
    long  last_epoch()   const { return epoch_.back(); };
    // the length of the track 
	long  track_length() const { return position_.size(); }; 
	// get track attributes based on sonar configuration
    void get_track_attributes(float start_range,   
                              float range_step,    
                              float start_bearing, 
                              float bearing_step,  
                              float ping_rate,
                              TrackAttributes& attr);


};


#endif // __NIMS_TRACKED_OBJECT_HPP__
