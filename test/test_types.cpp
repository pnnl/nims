#include <iostream>
#include <opencv2/opencv.hpp>

#define NIMS_LOG_DEBUG std::cout

#include "pixelgroup.h"
#include "detections.h"
//#include "tracked_object.h"
//#include "tracks_message.h"


using namespace std;
using namespace cv;


float range_min, brg_min, el_min;
float range_max, brg_max, el_max;
int num_samples, num_beams, num_el;
float range_step, brg_step, el_step;

/*
device = Kongsberg M3 Multibeam sonar
   version = 8
   ping_num = 366
   ping_sec = 1438886273
   ping_millisec = 628
   soundspeed_mps = 1502
   num_samples = 1373
   range_min_m = 0.4
   range_max_m = 20
   winstart_sec = 0.000538
   winlen_sec = 0.028109
   num_beams = 108
   beam_angles_deg = -70.038 to 70.038
   freq_hz = 500000
   pulselen_microsec = 50
   pulserep_hz = 28
*/
void M3()
{
	range_min = 2.0;
	brg_min = -59.5313;
	el_min = 0.0;
	range_max = 50.0;
	brg_max = 59.5313;
	el_max = 0.0;
	num_samples = 1681;
	num_beams = 128;
	num_el = 1;

	range_step = (range_max - range_min)/(num_samples-1);
	brg_step = (brg_max - brg_min)/(num_beams-1); 
	el_step = 0.0;
}

void Generic()
{
	range_min = 0.1;
	brg_min = -60.0;
	el_min = 0.0;
	range_max = 20.0;
	brg_max = 60.0;
	el_max = 0.0;
	num_samples = 200;
	num_beams = 121;
	num_el = 1;

	range_step = (range_max - range_min)/(num_samples-1);
	brg_step = (brg_max - brg_min)/(num_beams-1); 
	el_step = 0.0;
}

void print_all()
{
cout << endl;
cout << "range_min = " << range_min << ", range_max = " << range_max << ", N = " << num_samples << endl;
cout << "brg_min = " << brg_min << ", brg_max = " << brg_max << ", N = " << num_beams << endl;
cout << "el_min = " << el_min << ", el_max = " << el_max << ", N = " << num_el << endl;

float range_max_calc = range_min + (num_samples-1) * range_step;
float brg_max_calc = brg_min + (num_beams-1) * brg_step;
float el_max_calc = el_min + (num_el-1) * el_step;

cout << endl;
cout << "range_step = " << range_step << ", range_max_calc = " << range_max_calc << endl;
cout << "brg_step = " << brg_step << ", brg_max_calc = " << brg_max_calc << endl;
cout << "el_step = " << el_step << ", el_max_calc = " << el_max_calc << endl;

}

void print_detection(const Detection& d)
{
	cout << "center: " << d.center[BEARING] << ", " << d.center[RANGE] << ", " << d.center[ELEVATION] << endl;
	cout << "size: " << d.size[BEARING] << ", " << d.size[RANGE] << ", " << d.size[ELEVATION] << endl;
	cout << "rotation: " << d.rot_deg[0] << ", " << d.rot_deg[1] << endl;
	//cout << "mean intensity: " << d.mean_intensity << endl;
	//cout << "max instensity: " << d.max_intensity << endl;
}

// 2x3 horizontal rectangle
void make_horiz_rect(const Point2i& offset, vector<Point2i>& hrect)
{
hrect.clear();
for (int x=0;x<3;++x)
{
	hrect.push_back(Point2i(x,0)+offset);
	hrect.push_back(Point2i(x,1)+offset);
}
}

// 3x2 vertical rectangle
void make_vert_rect(const Point2i& offset, vector<Point2i>& vrect)
{
	vrect.clear();
for (int y=0;y<3;++y)
{
	vrect.push_back(Point2i(0,y)+offset);
	vrect.push_back(Point2i(1,y)+offset);
}
}

// 3x2 vertical rotated rectange
void make_rotated_rect(const Point2i& offset, vector<Point2i>& rvrect)
{
rvrect.clear();
rvrect.push_back(Point2i(2,0)+offset);
rvrect.push_back(Point2i(1,1)+offset);
rvrect.push_back(Point2i(2,1)+offset);
rvrect.push_back(Point2i(0,2)+offset);
rvrect.push_back(Point2i(1,2)+offset);
rvrect.push_back(Point2i(0,3)+offset);
}

/*
std::ostream& operator<<(std::ostream& strm, const Track& trk)
{
	strm  
	<< "id = " << trk.id << endl          // unique track identifier
	<< "size_sq_m = " << trk.size_sq_m  << endl     // avg size in square meters
	<< "size_sq_m = " << trk.size_sq_m << endl      // avg size in square meters
	<< "speed_mps = " << trk.speed_mps << endl      // avg speed in meters per sec.
	<< "target_strength = " << trk.target_strength << endl// avg intensity

	<< "min_range_m = " << trk.min_range_m  << endl   // bounding box of the entire track
	<< "max_range_m = " << trk.max_range_m << endl
	<< "min_bearing_deg = " << trk.min_bearing_deg << endl
	<< "max_bearing_deg = " << trk.max_bearing_deg << endl
    << "min_elevation_deg = " << trk.min_elevation_deg << endl
	<< "max_elevation_deg = " << trk.max_elevation_deg << endl

    << "first_detect = " << trk.first_detect << endl   // time when track started
	<< "pings_visible = " << trk.pings_visible << endl// number of pings the target was visible

	<< "last_pos_range = " << trk.last_pos_range  << endl // last observed position
	<< "last_pos_bearing = " << trk.last_pos_bearing << endl
	<< "last_pos_elevation = " << trk.last_pos_elevation << endl

	<< "last_vel_range = " << trk.last_vel_range  << endl // last observed velocity
	<< "last_vel_bearing = " << trk.last_vel_bearing << endl
	<< "last_vel_elevation = " << trk.last_vel_elevation << endl

	<< "width = " << trk.width   << endl         // last observed size
	<< "length = " << trk.length << endl
	<< "height = " << trk.height << endl
	<< endl;

	return strm;

}
*/
int main (int argc, char * argv[]) 
{

// set up frame buffer like M3
M3();
//Generic();

print_all();

//*************************************************
// Test PixelGrouping to Detection
//  
// PixelGrouping is vector<Point2i>
// Detection is the center and size of the 
// PixelGrouping in world coordinages (range, bearing, elevation)
//

// create PixelToWorld coordinate transform from M3 
PixelToWorld ptw(range_min, brg_min, el_min,
                 range_step, brg_step, el_step);

// test points
// near range, port
Point2i np(0,0);
// far range, starboard
Point2i fs(num_beams-1, num_samples-1);
// mid range, center
Point2i ctr((int)(num_beams/2), (int)(num_samples/2));

float ts = 0.0;
vector<Point2i> points;

cout << endl;
cout << "NEAR, PORT:" << endl;
points.clear();
points.push_back(np);
Detection det1(ts, points, ptw);
print_detection(det1);

cout << endl;
cout << "FAR, STARBOARD:" << endl;
points.clear();
points.push_back(fs);
Detection det2(ts, points, ptw);
print_detection(det2);
cout << endl;

cout << "CENTER:" << endl;
points.clear();
points.push_back(ctr);
Detection det3(ts, points, ptw);
print_detection(det3);

Point2i offset(num_beams - 20,num_samples-40);
cout << endl << "offset: x = " << offset.x << ", y = " << offset.y << endl;

// 2x3 horizontal rectangle
vector<Point2i> hrect;
make_horiz_rect(offset, hrect);

// 3x2 vertical rectangle
vector<Point2i> vrect;
make_vert_rect(offset,vrect);

// 3x2 vertical rotated rectange
vector<Point2i> rvrect;
make_rotated_rect(offset, rvrect);

cout << endl;
cout << "HORIZONTAL RECT:" << endl;
Detection det_hr(ts, hrect, ptw);
print_detection(det_hr);
cout << endl;

cout << endl;
cout << "VERTICAL RECT:" << endl;
Detection det_vr(ts, vrect, ptw);
print_detection(det_vr);
cout << endl;

cout << endl;
cout << "ROTATED RECT:" << endl;
Detection det_rr(ts, rvrect, ptw);
print_detection(det_rr);
cout << endl;

//*************************************************
// Test PixelGrouping to Detection
// 

// binary image
Mat imb(num_samples, num_beams, CV_32FC1, Scalar(0.0));

cout << "Expecting NEAR,PORT FAR,STARBOARD and CENTER" << endl;

imb.at<float>(np) = 1.0;
imb.at<float>(fs) = 1.0;
imb.at<float>(ctr) = 1.0;

int min_size = 1;
PixelGrouping objects;
group_pixels(Mat(imb>0.0), min_size, objects);
int n_obj = objects.size();
cout << endl << " number of detected objects: " << n_obj << endl;
cout << endl;

double timestamp = 0.0;
for (int k=0; k<n_obj; ++k)
{
    // convert pixel grouping to detections
    Detection det(timestamp, objects[k], ptw);
	print_detection(det);
	cout << endl;
}
imb.at<float>(np) = 0.0;
imb.at<float>(fs) = 0.0;
imb.at<float>(ctr) = 0.0;

cout << "Expecting ROTATED RECT" << endl;
for (int p=0; p<rvrect.size(); ++p)
{
	imb.at<float>(rvrect[p]) = 1.0;
}

group_pixels(Mat(imb>0.0), min_size, objects);
n_obj = objects.size();
cout << endl << " number of detected objects: " << n_obj << endl;
cout << endl;
for (int k=0; k<n_obj; ++k)
{
    // convert pixel grouping to detections
    Detection det(timestamp, objects[k], ptw);
	print_detection(det);
	cout << endl;
}

/*
//*************************************************
// Test Detection to TrackedObject
//  
// TrackedObject is vector<Detection>
//

int id = 0;
float epoch = 0.0;
TrackedObject obj(id, epoch, det_hr);
//Detection det_new;
vector<Detection> pred;
pred.push_back(obj.predict(epoch));
for (int t=0; t<4; ++t)
{
	offset = Point2i(20 + 2*t, 20 + 3*t);
	make_horiz_rect(offset, hrect);
	epoch++;
	obj.update(epoch, Detection(epoch, hrect, ptw));
	pred.push_back(obj.predict(epoch));

}
vector<Detection> trk(obj.get_track());
cout << endl << "size of track is " << trk.size() << " detections" << endl;

for (int t=0; t<trk.size(); ++t)
{
cout << endl;
cout << t << ":" << endl;
print_detection(trk[t]);
cout << "pred--------" << endl;
print_detection(pred[t]);
}

cout << endl;

//*************************************************
// Test TrackedObject to TracksMessage
//  
long frame_num =7; 
long ping_num = 77;
vector<Track> tracks;
tracks.push_back( Track(obj.get_id(), obj.get_track()) );
tracks.push_back( Track(obj.get_id(), obj.get_track()) );
tracks.push_back( Track(obj.get_id(), obj.get_track()) );
cout << endl << tracks[0];
TracksMessage msg_trks(frame_num, ping_num, ts, tracks);
cout << endl << "created message with " << msg_trks.num_tracks << " tracks" << endl;
for (int k=0;k<msg_trks.num_tracks;++k)
{
	cout << endl << "track " << k << ":" << endl;
	cout << msg_trks.tracks[k];
}

*/
return 0;
}