//
//  tracked_object.cpp
//  
//
//  Created by Shari Matzner on 6/18/2015.
//  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
//

#include "tracked_object.h"

using namespace cv;
using namespace std;

void TrackedObject::init_tracking(int Nstate, int Nmeasure, float q, float r)
{
    kf_.init(Nstate, Nmeasure);
    // for extended Kalman filter
    //kf_.transitionMatrix = *(Mat_<float>(Nstate, Nstate) << 1, 1, 0, 1);
    Mat F = Mat::eye(Nstate, Nstate, CV_32FC1);
    F.at<float>(0,1) = 1;
    F.at<float>(2,3) = 1;
    F.copyTo(kf_.transitionMatrix);
    
    kf_.measurementMatrix.at<float>(0,0) = 1.0;
    kf_.measurementMatrix.at<float>(1,2) = 1.0;

    setIdentity(kf_.processNoiseCov, Scalar::all(q)); // values from example
    kf_.processNoiseCov.at<float>(0,1) = q;
    kf_.processNoiseCov.at<float>(1,0) = q;
    kf_.processNoiseCov.at<float>(2,3) = q;
    kf_.processNoiseCov.at<float>(3,2) = q;
    
    setIdentity(kf_.measurementNoiseCov, Scalar::all(r));
    
    setIdentity(kf_.errorCovPost, Scalar::all(1));
    //randn(kf_.statePost, Scalar::all(0), Scalar::all(0.1));
    kf_.statePost = Scalar::all(0);
    // TODO:  Set initial position.
    //kf_.statePost.at<float>(0,0) = initial_pos.x;
    //kf_.statePost.at<float>(2,0) = initial_pos.y;
}

/*
TrackedObject::TrackedObject(const Point2f& initial_pos, InputArray initial_image, int initial_epoch, float process_noise, float measurement_noise)
{
    Mat initial_image_ = initial_image.getMat();
    CV_Assert( initial_image_.type() == CV_32FC1 || initial_image_.type() == CV_8UC1 || initial_image_.type() == CV_8UC3 || initial_image_.empty() );

	epoch_.push_back(initial_epoch);
	position_.push_back(initial_pos);
	image_.push_back(initial_image_);

    int Nstate = 4; // x, vx, y, vy
    int Nmeasure = 2; // x, y
    
    init_tracking(Nstate, Nmeasure, process_noise, measurement_noise);
	
}
*/
//TrackedObject::TrackedObject(long id, float epoch, Detection initial_det, float process_noise, float measurement_noise)
TrackedObject::TrackedObject(long id, Detection initial_det, float process_noise, float measurement_noise)
{
    id_ = id;
    //epoch_.push_back(epoch);
    
    int Nstate = 4; // x, vx, y, vy
    int Nmeasure = 2; // x, y
    
    init_tracking(Nstate, Nmeasure, process_noise, measurement_noise);
    kf_.statePost.at<float>(0,0) = initial_det.center[0];
    kf_.statePost.at<float>(2,0) = initial_det.center[1];

    detections_.push_back(initial_det);

}


//void TrackedObject::update(float epoch, Detection new_det)
void TrackedObject::update(Detection new_det)
{
    //epoch_.push_back(epoch);
    detections_.push_back(new_det);

    Mat_<float> measurement(2,1);
    measurement(0) = new_det.center[0];
    measurement(1) = new_det.center[1];
    kf_.correct(measurement);

}


Detection TrackedObject::predict(double epoch)
{
    Mat prediction = kf_.predict();
    // elapsed time
    //float dt = epoch - epoch_.back();
    float dt = epoch - detections_.back().timestamp;
    kf_.transitionMatrix.at<float>(0,1) = dt;
    kf_.transitionMatrix.at<float>(2,3) = dt;

    Detection predicted_det;
    predicted_det.center[0] = prediction.at<float>(0);
    predicted_det.center[1] = prediction.at<float>(2);

    return predicted_det;
}

/*
void  TrackedObject::get_track(vector<long>& epoch, vector<Point2f>& position, std::vector<cv::Mat>* img) const
{
        epoch.assign(epoch_.begin(), epoch_.end());
        position.assign(position_.begin(), position_.end());
        if (img != NULL) img->assign(image_.begin(), image_.end());
}

void  TrackedObject::get_track(std::vector<long>& epoch, std::vector<Rect>& bounding_box) const
{
    epoch.assign(epoch_.begin(), epoch_.end());
    bounding_box.clear();
    int N = epoch_.size();
    for (int k=0; k<N; ++k)
    {
        Rect bb(position_[k].x, position_[k].y, image_[k].cols, image_[k].rows);
        bounding_box.push_back(bb);
    }
}


void  TrackedObject::get_track_smoothed(vector<long>& epoch, vector<Point2f>& position) const
{
	epoch.assign(epoch_.begin(), epoch_.end());
	int N = epoch_.size();
	//cout << endl << "smoothing track of length " << N << endl;
	if (N < 3)
	{
		position.assign(position_.begin(), position_.end());
		return;
	}
	position.assign(N,Point2f(0,0));
	int win_size = (N<7) ? 3 : 5;
	//cout << "win_size = " << win_size << endl;
	int half_win = (win_size-1)/2;
	//cout << "half_win = " << half_win << endl;
	float one_over_winsize = 1.f/win_size;
	// first elements with padding
	for (int k=0;k<half_win;++k)
	{
		position[k] = position_[k];
		}
	for (int k=half_win;k<N-half_win;++k)
	{
		for (int j=k-half_win;j<k+half_win+1;++j)
			position[k]+=position_[j];
		position[k] = position[k]*one_over_winsize;
	}
	//last elements with padding
	for (int k=N-half_win;k<N;++k)
	{
		position[k] = position_[k];
		}
}
*/

/*
void print_attribute_labels(std::ostream& strm)
{
    strm << "Track ID, "
    << "First Ping, "
    << "Last Ping, "
    << "First Range (m), "
    << "Last Range (m), "
    << "First Bearing (deg), "
    << "Last Bearing (deg), "
    << "Min Range (m), "
    << "Max Range (m), "
    << "Min Bearing (deg), "
    << "Max Bearing (deg), "
    << "Pings Visible, "
    << "Max Run, "
    //<< "Mean Intensity (dB), "
    //<< "Std Intensity (dB), "
    << "Mean Speed (m/s), "
    << "Std Speed (m/s), "
    ;
    
}
*/



