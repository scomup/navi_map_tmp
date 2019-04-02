#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/passthrough.h>
#include <pcl/surface/mls.h>
#include <pcl/surface/impl/mls.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/common/transforms.h>
#include <ros/ros.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include <pcl_conversions/pcl_conversions.h>


#include <Eigen/Eigen>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/SVD>

#include <unordered_map>
#include <utility>
	

#include "Octree.h"


#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Pose.h>
#include <nav_msgs/Path.h>




pcl::PointCloud<pcl::PointXYZI>::Ptr LoadPcdFile(std::string file_name)
{
    pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>);
    std::cout << "try to read " << file_name << " ." << std::endl;
    if (pcl::io::loadPCDFile<pcl::PointXYZI>(file_name, *cloud) == -1)
    {
        PCL_ERROR("Couldn't read file test_pcd.pcd \n");
        abort();
    }
    std::cout << "read " << file_name << " success!" << std::endl;
    return cloud;
}

void createMarker(GlobalPlan::OctreeNode *node, visualization_msgs::MarkerArray& marker_array)
{
    double level = node->level;
    static int id = 0;
    auto centroid = node->centroid;
    auto covariance = node->covariance;

    Eigen::JacobiSVD<Eigen::Matrix3d> svd(covariance, Eigen::ComputeFullU | Eigen::ComputeFullV);

    if (std::isnan(svd.singularValues().x()) ||
        std::isnan(svd.singularValues().y()) ||
        std::isnan(svd.singularValues().z()))
    {
        return;
    }

    bool isplane = svd.singularValues().z() / svd.singularValues().y() < 0.2;
    bool islinear = svd.singularValues().y() / svd.singularValues().x() < 0.2;
    bool good = (isplane || islinear);

    if (!good && level != 0)
    {
        for (auto cnode : node->cnode)
        {
                createMarker(cnode, marker_array);
                return;
            
        }
    }

    Eigen::Matrix3d R(svd.matrixU());
    R.col(0).normalize();
    R.col(1).normalize();
    R.col(2) = R.col(0).cross(R.col(1));
    R.col(2).normalize();
    R.col(0) = R.col(1).cross(R.col(2));
    R.col(0).normalize();
    Eigen::Quaterniond q(R);

    visualization_msgs::Marker marker;
    marker.header.frame_id = "/map";
    marker.header.stamp = ros::Time::now();
    marker.ns = "basic_shapes" + std::to_string(id);
    marker.id = id++;
    marker.type = visualization_msgs::Marker::SPHERE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.lifetime = ros::Duration();

    marker.scale.x = sqrt(svd.singularValues().x()) * 5;
    marker.scale.y = sqrt(svd.singularValues().y()) * 5;
    marker.scale.z = sqrt(svd.singularValues().z()) * 5;

    marker.pose.position.x = centroid[0];
    marker.pose.position.y = centroid[1];
    marker.pose.position.z = centroid[2];
    marker.pose.orientation.x = q.x();
    marker.pose.orientation.y = q.y();
    marker.pose.orientation.z = q.z();
    marker.pose.orientation.w = q.w();
    if (good)
    {
        marker.color.r = 0.0f;
        marker.color.g = 1;
        marker.color.b = 0;
        marker.color.a = 0.3f;
    }
    else
    {
        marker.color.r = 1.0f;
        marker.color.g = 0;
        marker.color.b = 0;
        marker.color.a = 0.3f;
    }

    if(level != 1)
    marker_array.markers.push_back(marker);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "info_marker_publisher1");
    ros::NodeHandle nh;


    pcl::PointCloud<pcl::PointXYZI>::Ptr cloud_raw = LoadPcdFile("/home/liu/gazebo.pcd");
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
    pcl::copyPointCloud(*cloud_raw, *cloud);
    GlobalPlan::Octree<pcl::PointXYZ> octree;//(voxel_ids, cloud);
    octree.setInput(cloud);
    auto nodes = octree.getLevelNode(1);

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_map(new pcl::PointCloud<pcl::PointXYZ>());

    visualization_msgs::MarkerArray marker_array;
    for (auto n : nodes)
    {
        createMarker(n, marker_array);
    }

    sensor_msgs::PointCloud2 output;
    pcl::toROSMsg(*cloud_map, output);
    output.header.frame_id = "map";

    auto cell_pub = nh.advertise<sensor_msgs::PointCloud2>("points", 100, true);
    cell_pub.publish(output);

    ros::Publisher marker_pub = nh.advertise<visualization_msgs::MarkerArray>("marker", 1, true);
    marker_pub.publish(marker_array);

    ros::spin();



  
    /*
    std::vector<Eigen::Vector3i> voxel_ids(cloud->points.size());

    for (int i = 0; i < (int)cloud->points.size(); i++)
    {
        Eigen::Vector3i &vid = voxel_ids[i];
        pcl::PointXYZ p = cloud->points[i];

        vid(0) = static_cast<int>(floor(p.x / 0.1));
        vid(1) = static_cast<int>(floor(p.y / 0.1));
        vid(2) = static_cast<int>(floor(p.z / 0.1));
    }
    std::cout<<"1.."<<std::endl;

    GlobalPlan::Octree<pcl::PointXYZ> octree;//(voxel_ids, cloud);
    octree.setInput(voxel_ids, cloud);
    std::cout<<"OK.."<<std::endl;

    auto nodes = octree.getOctreeLevel(0);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_map(new pcl::PointCloud<pcl::PointXYZ>());

    for (auto n : nodes)
    {
        cloud_map->push_back(pcl::PointXYZ(n.centroid(0), n.centroid(1), n.centroid(2)));
    }

    sensor_msgs::PointCloud2 output;
    pcl::toROSMsg(*cloud_map, output);
    output.header.frame_id = "map";

    auto cell_pub = nh.advertise<sensor_msgs::PointCloud2>("points", 100, true);
    cell_pub.publish(output);
        ros::spin();
        */


    //GlobalPlan::NaviMapVisualization map_visual(&navi_map);

    /*
    auto s = navi_map.FindNearestCell(Eigen::Vector3d(0,-1.5,0));
    auto g = navi_map.FindNearestCell(Eigen::Vector3d(0,15,3));
    std::vector<Eigen::Vector3d> path;
    navi_map.FindPath(s, g, path);
    ros::Publisher plan_pub;
    plan_pub = nh.advertise<nav_msgs::Path>("global_plan", 1, true);
    nav_msgs::Path path_msg;

    path_msg.poses.resize(path.size());
    path_msg.header.frame_id = "map";
    path_msg.header.stamp = ros::Time::now();


    //graph_->FindPath(start_idx, goal_idx, std::vector<uint> path);
    int i = 0;
    for (Eigen::Vector3d p : path)
    {
        geometry_msgs::PoseStamped pose;
        pose.pose.position.x = p.x();
        pose.pose.position.y = p.y();
        pose.pose.position.z = p.z()+0.3;
        path_msg.poses[i++] = pose;
    }
    plan_pub.publish(path_msg);

    auto cell_pub = nh.advertise<sensor_msgs::PointCloud2>("points", 100, true);
    cell_pub.publish(map_visual.createCellMarker());

    ros::Publisher marker_pub = nh.advertise<visualization_msgs::MarkerArray>("marker", 1, true);
    marker_pub.publish(map_visual.createCovarianceMarker());

    std::cout<<"OK!"<<std::endl;

    ros::spin();
*/
    return 0;
}