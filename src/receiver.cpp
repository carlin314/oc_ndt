#include "ros/ros.h"
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <assert.h>
#include <cstddef>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
//pcl
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>
#include <pcl_ros/transforms.h>
#include <pcl/io/io.h>
#include <pcl/io/ply_io.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/features/normal_3d.h>
#include <pcl/common/transforms.h>
//#include "map2D.h"
//#include "AstarPlanar.h"
#include "GlobalPlan.h"
#include <vector>
#include <fstream>
#include"omp.h"
using namespace Eigen;
using namespace std;
using namespace daysun;
typedef multimap<string,daysun::OcNode *>  MAP_INT_MORTON_MULTI;
typedef multimap<string,daysun::OcNode *>::iterator iterIntNode;

RobotSphere robot(0.25); //radius--variable--according to the range of map
//test-0.25, sys-0.125
daysun::TwoDmap map2D(robot.getR());
ros::Publisher marker_pub,change_pub,markerArray_pub,marker_pub_bo,route_pub;

//ofstream outfile("/home/daysun/testPointsSys.txt", ofstream::app);

void uniformDivision( const pcl::PointXYZ temp,const float r,bool change){
    string morton_xy,morton_z;
    Vec3 temp3(temp.x,temp.y,temp.z);
    map2D.transMortonXYZ(temp3,morton_xy,morton_z);
    //for change-record which xy have been changed
    if(change){
        if(map2D.changeMorton_list.size() != 0){
            list<string>::iterator it = find(map2D.changeMorton_list.begin(), map2D.changeMorton_list.end(), morton_xy);
            if (it == map2D.changeMorton_list.end()){ //not find
                map2D.changeMorton_list.push_back(morton_xy);
            }
        }else{
            map2D.changeMorton_list.push_back(morton_xy);
        }
//        cout<<"change morton: "<<morton_xy<<","<<morton_z<<endl;
    }

    if(map2D.map_xy.count(morton_xy) == 0){ //not found
        //add new node
        daysun::OcNode * node = new daysun::OcNode();
        node->lPoints.push_back(temp);
        node->morton = morton_xy;
        node->z = morton_z;
        map2D.map_xy.insert(MAP_INT_MORTON_MULTI::value_type(morton_xy,node));
        map2D.map_z.insert(MAP_INT_MORTON_MULTI::value_type(morton_z,node));
        map2D.morton_list.push_back(morton_xy);       
    }else{//find        
                    iterIntNode beg = map2D.map_xy.lower_bound(morton_xy);
                     iterIntNode end = map2D.map_xy.upper_bound(morton_xy);
                     bool found = false;
                     while(beg != end){
                         if( (beg->second)->z.compare(morton_z) == 0 ){
                             (beg->second)->lPoints.push_back(temp);
                             found = true;
                             break;
                         }
                         ++beg;
                     }
                     if(!found){
                         daysun::OcNode * node = new daysun::OcNode();
                         node->lPoints.push_back(temp);
                         node->morton = morton_xy;
                         node->z = morton_z;
                         map2D.map_xy.insert(MAP_INT_MORTON_MULTI::value_type(morton_xy,node));
                         map2D.map_z.insert(MAP_INT_MORTON_MULTI::value_type(morton_z,node));
//                         map2D.morton_list.push_back(morton_xy);                    
                     }
    }
}


void chatterCallback(const sensor_msgs::PointCloud2::ConstPtr & my_msg)
{
    cout<<"receive initial\n";
    double time_start = stopwatch();
    pcl::PCLPointCloud2 pcl_pc2;
    pcl_conversions::toPCL(*my_msg,pcl_pc2);
    pcl::PointCloud<pcl::PointXYZ>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromPCLPointCloud2(pcl_pc2,*temp_cloud);
    cout<<temp_cloud->points.size()<<endl;
    map2D.setCloudFirst(Vec3(temp_cloud->points[0].x,temp_cloud->points[0].y,temp_cloud->points[0].z));

#pragma omp parallel for
    for (int i=1;i<temp_cloud->points.size();i++)
    {
//        outfile<<temp_cloud->points[i].x<<","<<temp_cloud->points[i].y<<","<<temp_cloud->points[i].z<<endl;
        uniformDivision(temp_cloud->points[i],robot.getR(),false);
     }
     double time_end = stopwatch();
//    outfile.close();
    cout<<"division done. Time cost: "<<(time_end-time_start)<<" s\n";
    cout<<"morton size: "<<map2D.morton_list.size()<<endl;
     double time_start1 = stopwatch();
    map2D.create2DMap();
     double time_end1 = stopwatch();
     cout<<"2D Map creation done. Time cost: "<<(time_end1-time_start1)<<" s\n";
    if(marker_pub.getNumSubscribers()){
        map2D.showInital(marker_pub,robot,0);
        map2D.showBottom(marker_pub_bo,robot.getR());
        cout<<"initial show done\n";
    }    

    Vec3 goal(robot.getGoal());
    map2D.computeCost(goal,robot,markerArray_pub); //compute the cost map

    AstarPlanar globalPlanr(robot.getPosition(),robot.getGoal());
    if(globalPlanr.findRoute(map2D,robot))
        globalPlanr.showRoute(map2D,route_pub,robot.getR());
}


///change points
void changeCallback(const sensor_msgs::PointCloud2::ConstPtr & my_msg){
     cout<<"receiveChange\n";
     pcl::PCLPointCloud2 pcl_pc2;
     pcl_conversions::toPCL(*my_msg,pcl_pc2);
     pcl::PointCloud<pcl::PointXYZ>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZ>);
     pcl::fromPCLPointCloud2(pcl_pc2,*temp_cloud);

     map2D.changeMorton_list.clear();
     for (int i=1;i<temp_cloud->points.size();i++)
     {
         uniformDivision(temp_cloud->points[i],robot.getR(),true);
      }
     cout<<"change division done\n";
     cout<<"change morton size: "<<map2D.changeMorton_list.size()<<endl;
     map2D.change2DMap();
     map2D.showInital(change_pub,robot,1);
}


int main(int argc, char **argv)
{
  ros::init(argc, argv, "fullSys_listener");
  ros::start();
  ros::NodeHandle n;
  marker_pub = n.advertise<visualization_msgs::MarkerArray>("initial_marker_array", 1000);
  markerArray_pub = n.advertise<visualization_msgs::MarkerArray>("traversibility_marker_array", 1000);
  change_pub = n.advertise<visualization_msgs::MarkerArray>("change_marker_array", 1000);
  marker_pub_bo = n.advertise<visualization_msgs::MarkerArray>("bottom_marker_array", 1000);
  route_pub= n.advertise<visualization_msgs::MarkerArray>("route_marker_array", 1000);
  ros::Subscriber sub = n.subscribe("publisher/cloud_fullSys", 1000, chatterCallback); //initial
  ros::Subscriber sub_change = n.subscribe("publisher/cloud_change", 1000,changeCallback);//change
  ros::spin();
  ros::shutdown();
  return 0;
}

