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
#include <opencv2/video/tracking.hpp>

// A track in 2D space.  A track is a sequence of steps, where
// each step consists of a location in space and a time.  
// The steps are ordered by epoch (time); new steps are added to the end of the track.
// The number of steps is not known in advance.  Epoch may be clock time, video frame,
// or any aribtrary index.
/*
class Track2D
{
	std::vector<cv::Point2f> position_;
	std::vector<long>        epoch_;
	
public:
    Track2D(const cv::Point2f& initial_pos, long initial_epoch = 0);
	void update(const cv::Point2f& newpos,  long epoch); // add a new step to the track
	int length() const { return position_.size(); };  // returns the number of steps in the track
	bool empty() const { return position_.empty(); }; // true if the track contains no steps
	long last_epoch() const { return epoch_.back(); }; // return the epoch of the last update
	
};
*/

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
    void        update(long epoch, const cv::Point2f& new_pos, cv::InputArray new_image = cv::noArray() ); // add a new step to the track
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

#endif // __NIMS_TRACKED_OBJECT_HPP__
