//
//  tracked_object.h
//  
//
//  Created by Shari Matzner on 6/18/2015.
//  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
//

#ifndef __NIMS_TRACKED_OBJECT_HPP__
#define __NIMS_TRACKER_HPP__

#include <iostream>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp> // kalman filter


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
	//void  get_track(vector<long>& epoch, vector<cv::Point2f>& position) const { epoch.assign(epoch_); position.assign(position_); }; // get a copy of the current track
	void  get_track(std::vector<long>& epoch, std::vector<cv::Point2f>& position, std::vector<cv::Mat>* img) const;
	void  get_track(std::vector<long>& epoch, std::vector<cv::Rect>& bounding_box) const;
	void  get_track_smoothed(std::vector<long>& epoch, std::vector<cv::Point2f>& position) const;
	void  get_last_image(cv::OutputArray lastimg) const; // return the last image of the track
    long  last_epoch()   const { return epoch_.back(); }; // return the epoch of the last update
	long  track_length() const { return position_.size(); }; // return the length of the track

};

struct TrackAttributes {
    long track_id;
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
void get_track_attributes(const std::vector<long>& framenum,
                          const std::vector<cv::Point2f>& img_pos,
                          //const std::vector<cv::Mat>& img,
                          float start_range,   // range of first sample in ping data
                          float range_step,    // delta range for each sample
                          float start_bearing, // beam angle of first beam in ping data
                          float bearing_step,  // delta angle of each beam
                          float ping_rate,
                          TrackAttributes& attr);

std::ostream& operator<<(std::ostream& strm, const TrackAttributes& attr);
void print_attribute_labels(std::ostream& strm);



#endif // __NIMS_TRACKED_OBJECT_HPP__
