/**
 * Implementation of Cluster class.
*
* @author: Kostas Konstantinidis
* @date: 14.03.2019
*/

#include "cluster.h"


Cluster::Cluster(unsigned long int id, const pointList& new_points, const double& dt ){

  this->id = id;
  this->r = rand() / double(RAND_MAX);
  this->g = rand() / double(RAND_MAX);
  this->b = rand() / double(RAND_MAX);
  this->moving = true; //all clusters at the beginning are init moving

  // Initialization of Kalman Filter
  int n = 4; // Number of states
  int m = 4; // Number of measurements
  // double dt = 0.1;  // Time step
  MatrixXd A(n, n); // System dynamics matrix
  MatrixXd C(m, n); // Output matrix
  MatrixXd Q(n, n); // Process noise covariance
  MatrixXd R(m, m); // Measurement noise covariance
  MatrixXd P(n, n); // Estimate error covariance
      
  A << 1, 0,dt, 0, 
       0, 1, 0,dt, 
       0, 0, 1, 0, 
       0, 0, 0, 1;

  C << 1, 0, 0, 0,
       0, 1, 0, 0,
       0, 0, 1, 0,
       0, 0, 0, 1;

  Q.setIdentity();
  R.setIdentity();
  P.setIdentity();

  KalmanFilter kalman_filter(dt, A, C, Q, R, P); // Constructor for the filter
  this->kf = kalman_filter;

  VectorXd x0(n);

  clusters.push_back(new_points);
  Cluster::calcMean(new_points);
  x0 << Cluster::meanX(), Cluster::meanY(), 0, 0;
  kf.init(0,x0, this->id);

  
}

void Cluster::update(const pointList& new_points, const double dt_in) {

  previous_mean_values = mean_values;

  clusters.push_back(new_points);
  Cluster::calcMean(new_points);
  this->dt = dt_in;
  calcRelativeVelocity();

  // Update Kalman Filter
  VectorXd y(4);
  y << meanX(), meanY(), vx, vy;

  kf.update(y, dt);
}

void Cluster::updateTrajectory(const tf::TransformListener& tf_listener) {

  if(tf_listener.canTransform(p_target_frame_name_, p_source_frame_name_, ros::Time())){

    pose_source_.header.stamp = ros::Time(0);
    pose_source_.pose.orientation.w = 1.0;
    pose_source_.header.frame_id = p_source_frame_name_;
    pose_source_.pose.position.x = kf.state()[0];
    pose_source_.pose.position.y = kf.state()[1];
    pose_source_.pose.orientation.w = 1; 

    geometry_msgs::PoseStamped pose_out;

    tf_listener.transformPose("map", pose_source_, pose_out);

    trajectory_.header.stamp = pose_out.header.stamp;
    trajectory_.header.frame_id = pose_out.header.frame_id;
    trajectory_.poses.push_back(pose_out);
    
    track_msg.header.stamp = pose_out.header.stamp;
    track_msg.header.frame_id = pose_out.header.frame_id;
    track_msg.id = this->id;
    track_msg.pose = pose_out.pose;
  } 
}

nav_msgs::Path Cluster::getTrajectory(){
  nav_msgs::Path empty_traj;
  if(!moving){return empty_traj;};
  return trajectory_;
}  


nav_msgs::Odometry Cluster::getOdom() {

  nav_msgs::Odometry odom;

  odom.header.stamp = ros::Time::now();
  odom.header.frame_id = "/laser";
  
  odom.pose.pose.position.x = meanX();
  odom.pose.pose.position.y = meanY();

  odom.twist.twist.linear.x = vx;
  odom.twist.twist.linear.y = vy;


  return odom;
}

nav_msgs::Odometry Cluster::getFilteredOdom() {

  nav_msgs::Odometry odom;


  odom.header.stamp = ros::Time::now();
  odom.header.frame_id = "/laser";
  
  odom.pose.pose.position.x = kf.state()[0];
  odom.pose.pose.position.y = kf.state()[1];

  odom.twist.twist.linear.x = kf.state()[2];
  odom.twist.twist.linear.y = kf.state()[3];


  return odom;
}

geometry_msgs::Pose Cluster::getPose() {

  geometry_msgs::Pose pose;

  pose.position.x = meanX();
  pose.position.y = meanY();
  pose.position.z = 0;


  pose.orientation.x = 0;
  pose.orientation.y = 0;
  pose.orientation.z = 0;
  pose.orientation.w = 0;

  return pose;
}

geometry_msgs::Pose Cluster::getVel() {

  geometry_msgs::Pose vel;

  vel.position.x = vx;
  vel.position.y = vy;
  vel.position.z = 0;


  vel.orientation.x = 0;
  vel.orientation.y = 0;
  vel.orientation.z = 0;
  vel.orientation.w = 0;

  return vel;
}

visualization_msgs::Marker Cluster::getArrowVisualisationMessage() {

  visualization_msgs::Marker arrow_marker;
 
  arrow_marker.type = visualization_msgs::Marker::ARROW;
  arrow_marker.header.frame_id = "/laser";
  arrow_marker.header.stamp = ros::Time::now();
  arrow_marker.ns = "velocities";
  arrow_marker.action = visualization_msgs::Marker::ADD;
  arrow_marker.pose.orientation.w = 1.0;    
  arrow_marker.scale.x = 0.2;
  arrow_marker.scale.y = 0.2;  
  arrow_marker.color.a = 1.0;
  arrow_marker.color.g = this->g;
  arrow_marker.color.b = this->b;
  arrow_marker.color.r = this->r;
  arrow_marker.id = this->id;
 
  geometry_msgs::Point p;
  p.x = meanX(); 
  p.y = meanY(); 
  p.z = 0;
  arrow_marker.points.push_back(p);

  p.x = meanX()+1; 
  p.y = meanY()+1; 
  p.z = 0;
  arrow_marker.points.push_back(p);
  return arrow_marker;
}

 visualization_msgs::Marker Cluster::getPointVisualisationMessage() {

  visualization_msgs::Marker fcorner_marker;

  fcorner_marker.type = visualization_msgs::Marker::POINTS;
  fcorner_marker.header.frame_id = "/laser";
  fcorner_marker.header.stamp = ros::Time::now();
  fcorner_marker.ns = "point";
  fcorner_marker.action = visualization_msgs::Marker::ADD;
  fcorner_marker.pose.orientation.w = 1.0;    
  fcorner_marker.scale.x = 0.1;
  fcorner_marker.scale.y = 0.1;  
  fcorner_marker.color.a = 1.0;
  fcorner_marker.color.g = this->g;
  fcorner_marker.color.b = this->b;
  fcorner_marker.color.r = this->r;
  fcorner_marker.id = this->id;

  geometry_msgs::Point p;
  p.x = meanX(); 
  p.y = meanY(); 
  p.z = 0;
  fcorner_marker.points.push_back(p);

  return fcorner_marker;
}

visualization_msgs::Marker Cluster::getClusterVisualisationMessage() {
  visualization_msgs::Marker cluster_vmsg;
  if(!moving){return cluster_vmsg;};//cluster not moving-empty msg
  cluster_vmsg.header.frame_id  = "laser";
  cluster_vmsg.header.stamp = ros::Time::now();
  cluster_vmsg.ns = "clusters";
  cluster_vmsg.action = visualization_msgs::Marker::ADD;
  cluster_vmsg.pose.orientation.w = 1.0;
  cluster_vmsg.type = visualization_msgs::Marker::POINTS;
  cluster_vmsg.scale.x = 0.13;
  cluster_vmsg.scale.y = 0.13;
  //cluster_vmsg.lifetime = ros::Duration(0.09);
  cluster_vmsg.id = this->id;

  cluster_vmsg.color.g = this->g;
  cluster_vmsg.color.b = this->b;
  cluster_vmsg.color.r = this->r;
  cluster_vmsg.color.a = 1.0;


  int last_cluster = clusters.size() -1;
  geometry_msgs::Point p;
 
  for(unsigned int j=0; j<clusters[last_cluster].size(); ++j){
    p.x = clusters[last_cluster][j].first;
    p.y = clusters[last_cluster][j].second;
    p.z = 0;
    cluster_vmsg.points.push_back(p);
  }

  return cluster_vmsg;
}


visualization_msgs::Marker Cluster::getLineVisualisationMessage() {

  visualization_msgs::Marker line_msg;

  if(!moving){return line_msg;};//cluster not moving-empty msg

  line_msg.header.stamp = ros::Time::now();
  line_msg.header.frame_id  = "/laser";
  line_msg.ns = "lines";
  line_msg.action = visualization_msgs::Marker::ADD;
  line_msg.pose.orientation.w = 1.0;
  line_msg.type = visualization_msgs::Marker::LINE_STRIP;
  line_msg.id = this->id;
  line_msg.scale.x = 0.1; //line width
  line_msg.lifetime = ros::Duration(0.09);
  line_msg.color.g = this->g;
  line_msg.color.b = this->b;
  line_msg.color.r = this->r;
  line_msg.color.a = 1.0;

  pointList last_cluster = clusters.back();


  //Feed the cluster into the Iterative End-Point Fit Function
  //and the l_shape_extractor and then save them into the l_shapes vector
  // Line and L-Shape Extraction


  vector<Point> pointListOut;
  Cluster::ramerDouglasPeucker(last_cluster, 0.1, pointListOut);
  geometry_msgs::Point p;
  for(unsigned int k =0 ;k<pointListOut.size();++k){
    p.x = pointListOut[k].first;
    p.y = pointListOut[k].second;
    p.z = 0;

    line_msg.points.push_back(p);
  }
  double l;
  if(pointListOut.size()>3){moving=false;};
  if(pointListOut.size()==1){
  for(unsigned int k=0; k<pointListOut.size()-1;++k){
    l = sqrt(pow(pointListOut[k+1].first - pointListOut[k].first,2) + pow(pointListOut[k+1].second - pointListOut[k].second,2));
    if(l>0.8){moving = false;};
  }
  }
  return line_msg;

}

visualization_msgs::Marker Cluster::getBoundingBoxVisualisationMessage() {
  
  visualization_msgs::Marker bb_msg;
  //if(!moving){return bb_msg;};//cluster not moving-empty msg

  bb_msg.header.stamp = ros::Time::now();
  bb_msg.header.frame_id  = "/laser";
  bb_msg.ns = "boundind_boxes";
  bb_msg.action = visualization_msgs::Marker::ADD;
  bb_msg.pose.orientation.w = 1.0;
  bb_msg.type = visualization_msgs::Marker::LINE_STRIP;
  bb_msg.id = this->id;
  bb_msg.scale.x = 0.05; //line width
  bb_msg.color.g = this->g;
  bb_msg.color.b = this->b;
  bb_msg.color.r = this->r;
  bb_msg.color.a = 1.0;
  
  float cx = 0;
  float cy = 0;
  //float phi = 0;
  float width = 0.3;
  float length = 0.6;
  
  geometry_msgs::Point p;
  p.x = cx + width/2;
  p.y = cy + length/2;
  bb_msg.points.push_back(p);
  p.x = cx + width/2;
  p.y = cy - length/2;
  bb_msg.points.push_back(p);
  p.x = cx - width/2;
  p.y = cy - length/2;
  bb_msg.points.push_back(p);
  p.x = cx - width/2;
  p.y = cy + length/2;
  bb_msg.points.push_back(p);
  p.x = cx + width/2;
  p.y = cy + length/2;
  bb_msg.points.push_back(p);
  return bb_msg;
}

void Cluster::calcMean(const pointList& c){

  double sum_x = 0, sum_y = 0;

  for(unsigned int i = 0; i<c.size(); ++i){

    sum_x = sum_x + c[i].first;
    sum_y = sum_y + c[i].second;
  }

    this->mean_values.first = sum_x / c.size();
    this->mean_values.second= sum_y / c.size();
}

void Cluster::calcRelativeVelocity(){

  this->vx = (mean_values.first - previous_mean_values.second)/dt;

  this->vy = (mean_values.first - previous_mean_values.second)/dt;

}

void Cluster::calcTheta(){

    this->theta = atan(this->mean_values.second / this->mean_values.second);
}

double Cluster::perpendicularDistance(const Point &pt, const Point &lineStart, const Point &lineEnd){
  //2D implementation of the Ramer-Douglas-Peucker algorithm
  //By Tim Sheerman-Chase, 2016
  //Released under CC0
  //https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm
  double dx = lineEnd.first - lineStart.first;
  double dy = lineEnd.second - lineStart.second;

  //Normalise
  double mag = pow(pow(dx,2.0)+pow(dy,2.0),0.5);
  if(mag > 0.0)
  {
    dx /= mag; dy /= mag;
  }

  double pvx = pt.first - lineStart.first;
  double pvy = pt.second - lineStart.second;

  //Get dot product (project pv onto normalized direction)
  double pvdot = dx * pvx + dy * pvy;

  //Scale line direction vector
  double dsx = pvdot * dx;
  double dsy = pvdot * dy;

  //Subtract this from pv
  double ax = pvx - dsx;
  double ay = pvy - dsy;

  return pow(pow(ax,2.0)+pow(ay,2.0),0.5);
}
 
void Cluster::ramerDouglasPeucker(const vector<Point> &pointList, double epsilon, vector<Point> &out){
  //2D implementation of the Ramer-Douglas-Peucker algorithm
  //By Tim Sheerman-Chase, 2016
  //Released under CC0
  //https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm

  // Find the point with the maximum distance from line between start and end
  double dmax = 0.0;
  size_t index = 0;
  size_t end = pointList.size()-1;
  for(size_t i = 1; i < end; i++)
  {
    double d = perpendicularDistance(pointList[i], pointList[0], pointList[end]);
    if (d > dmax)
    {
      index = i;
      dmax = d;
    }
  }

  // If max distance is greater than epsilon, recursively simplify
  if(dmax > epsilon)
  {
    // Recursive call
    vector<Point> recResults1;
    vector<Point> recResults2;
    vector<Point> firstLine(pointList.begin(), pointList.begin()+index+1);
    vector<Point> lastLine(pointList.begin()+index, pointList.end());
    ramerDouglasPeucker(firstLine, epsilon, recResults1);
    ramerDouglasPeucker(lastLine, epsilon, recResults2);
 
    // Build the result list
    out.assign(recResults1.begin(), recResults1.end()-1);
    out.insert(out.end(), recResults2.begin(), recResults2.end());
    if(out.size()<2)
      throw runtime_error("Problem assembling output");
  } 
  else 
  {
    //Just return start and end points
    out.clear();
    out.push_back(pointList[0]);
    out.push_back(pointList[end]);
  }
}

// float Cluster::lineSegmentExtractor(const vector<Point> &line_points, vector<double> &l_shape, bool visualise)
// {
//   if (line_points.size() == 3){

//     // l_shape.push_back(line_points[1].first); //xcorner
//     // l_shape.push_back(line_points[1].second);//ycorner
//     // double L1,L2,theta;
//     // L1 = sqrt(pow(line_points[1].first - line_points[0].first,2) + pow(line_points[1].second - line_points[0].second,2));
//     // L2 = sqrt(pow(line_points[2].first - line_points[1].first,2) + pow(line_points[2].second - line_points[1].second,2));
//     // l_shape.push_back(L1);
//     // l_shape.push_back(L2);
//     // theta = atan(line_points[1].second - line_points[0].second / line_points[1].first - line_points[0].first);//arctan(dy/dx)
//     // l_shape.push_back(theta);

//   }

//   else if (line_points.size() == 2){

//     if(abs(line_points[0].first - line_points[1].first) > line_points[0].second - line_points[1].second){
//       this-> Lx = sqrt(pwr(line_points[0].first - line_points[1].first,2) + pwr(line_points[0].firstsecond - line_points[1].second,2));
//     }
//     else{
//       this-> Ly = sqrt(pwr(line_points[0].first - line_points[1].first,2) + pwr(line_points[0].firstsecond - line_points[1].second,2));
//     }

//   }

//   return L;


//   // if(visualise == 1){

//   //   visualization_msgs::Marker lshapes_marker;
//   //   lshapes_marker.type = visualization_msgs::Marker::LINE_STRIP;

//   //   lshapes_marker.header.frame_id = "/map";
//   //   lshapes_marker.ns = "L-Shapes";
//   //   lshapes_marker.action = visualization_msgs::Marker::ADD;
//   //   lshapes_marker.id = cl;
//   //   lshapes_marker.pose.orientation.w = 1.0;    
//   //   lshapes_marker.header.stamp = ros::Time::now();
//   //   lshapes_marker.scale.x = 0.1;  //line width
//   //   lshapes_marker.color.g = 0.0f;
//   //   lshapes_marker.color.b = 0.0f;
//   //   lshapes_marker.color.r = 0.8f;
    

//   //   lshapes_marker.color.a = 1.0;

//   //   geometry_msgs::Point p;
//   //   for(unsigned int i=0;i<3;i++){
//   //     p.x = line_points[i].first;
//   //     p.y = line_points[i].second;
//   //     p.z = 0;
//   //     lshapes_marker.points.push_back(p);
//   //   }

//   // }
// }
//}
