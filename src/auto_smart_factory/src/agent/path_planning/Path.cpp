#include <utility>
#include <cmath>

#include "Math.h"
#include "ros/ros.h"
#include "visualization_msgs/Marker.h"
#include "agent/path_planning/Path.h"
#include "agent/path_planning/Map.h"

Path::Path(double startTimeOffset, std::vector<Point> nodes_, std::vector<double> waitTimes_, RobotHardwareProfile* hardwareProfile, double targetReservationTime, OrientedPoint start, OrientedPoint end) :
		startTimeOffset(startTimeOffset),
		nodes(std::move(nodes_)),
		waitTimes(std::move(waitTimes_)),
		hardwareProfile(hardwareProfile),
		targetReservationTime(targetReservationTime),
		start(start),
		end(end),
		isValidPath(true)
{
	if(nodes.size() != waitTimes.size()) {
		ROS_FATAL("nodes.size() != waitTimes.size()");
		isValidPath = false;
	}
	if(nodes.size() < 2) {
		ROS_FATAL("Tried to construct path with no node points");
		isValidPath = false;
	}
	if(nodes.at(0) == nodes.at(1)) {
		ROS_FATAL("Tried to construct path with identical points (point[0] == point[1]");
		isValidPath = false;
	}
	
	if(!isValidPath) {
		return;
	}

	// Turning time is considered as part of the following line segment.
	// Therefore, a line segment driving time = Time to turn to target Point + driving time to target point 

	duration = 0;
	batteryConsumption = 0;
	distance = 0;
	if(!nodes.empty()) {
		duration += waitTimes.at(0);
		batteryConsumption += hardwareProfile->getIdleBatteryConsumption(waitTimes.at(0));
		departureTimes.push_back(startTimeOffset + waitTimes.at(0));

		if(nodes.size() > 1) {
			for(unsigned long i = 1; i < nodes.size(); i++) { // last waitTime is not used
				double currentDistance = Math::getDistance(nodes.at(i - 1), nodes.at(i));

				// Don't compute angle difference for last node
				double angleDifference = 0;
				if(i < nodes.size() - 1) {
					angleDifference = Math::getAngleDifferenceInDegree(Math::getRotationInDeg(nodes[i] - nodes[i - 1]), Math::getRotationInDeg(nodes[i + 1] - nodes[i]));
				}

				distance += currentDistance;
				duration += hardwareProfile->getDrivingDuration(currentDistance);
				duration += waitTimes.at(i);

				batteryConsumption += hardwareProfile->getIdleBatteryConsumption(waitTimes.at(i));
				batteryConsumption += hardwareProfile->getDrivingBatteryConsumption(hardwareProfile->getDrivingDuration(currentDistance));
				batteryConsumption += hardwareProfile->getDrivingBatteryConsumption(hardwareProfile->getTurningDuration(angleDifference));

				// Departure times does not include turningTime as this is considered part of the next segment
				departureTimes.push_back(startTimeOffset + duration);
				duration += hardwareProfile->getTurningDuration(angleDifference);
			}
		}
	}
}

Path::Path() : 
	isValidPath(false)
{
}

const std::vector<Rectangle> Path::generateReservations(int ownerId) const {
	std::vector<Rectangle> reservations;

	double reservationSize = ROBOT_RADIUS * 2.0f;

	double currentTime = startTimeOffset;
	for(unsigned int i = 0; i < nodes.size() - 1; i++) {
		double currentDistance = Math::getDistance(nodes[i], nodes[i + 1]);
		Point currentDir = (nodes[i + 1] - nodes[i]) * 1.f * (1.f/currentDistance);
		double currentRotation = Math::getRotationInDeg(currentDir);
		double currentDuration = hardwareProfile->getDrivingDuration(currentDistance);

		// Waiting time - always for first node
		if(waitTimes.at(i) > 0 || i == 0) {
			reservations.emplace_back(nodes[i], Point(reservationSize, reservationSize), currentRotation, currentTime - reservationTimeMargin, currentTime + waitTimes[i] + reservationTimeMargin, ownerId);
			currentTime += waitTimes[i];
		}

		// Line segment
		auto segmentCount = static_cast<unsigned int>(std::ceil(currentDistance / maxReservationLength));
		double segmentLength = currentDistance / static_cast<float>(segmentCount);

		for(unsigned int segment = 0; segment < segmentCount; segment++) {
			auto segmentDouble = static_cast<double>(segment);
			
			Point startPos = nodes[i] + (segmentDouble * segmentLength * currentDir);
			Point endPos = nodes[i] + ((segmentDouble + 1.f) * segmentLength * currentDir);

			Point pos = (startPos + endPos) / 2.f;
			double startTime = currentTime - reservationTimeMargin + hardwareProfile->getDrivingDuration(segmentDouble * segmentLength);
			double endTime = currentTime + reservationTimeMargin + hardwareProfile->getDrivingDuration((segmentDouble + 1.f) * segmentLength);

			reservations.emplace_back(pos, Point(segmentLength + reservationSize, reservationSize), currentRotation, startTime, endTime, ownerId);
		}

		currentTime += currentDuration;
	}
	
	// path computation is responsible for checking that this area is free for the specified time
	if(targetReservationTime > 0) {
		double offset = 0.3f;
		double lengthMargin = 0.08f;
		double widthMargin = 0.2f;
		Point pos = nodes.back() + Math::getVectorFromOrientation(end.o) * offset;
		
		double length = (ROBOT_RADIUS + offset + lengthMargin) * 2.f;
		double width = (ROBOT_RADIUS + widthMargin) * 2.f;

		reservations.emplace_back(pos, Point(length, width), Math::toDeg(end.o), currentTime - reservationTimeMargin, currentTime + targetReservationTime + reservationTimeMargin, ownerId);
	}

	return reservations;
}

const std::vector<Point>& Path::getNodes() const {
	ROS_ASSERT(isValidPath);
	return nodes;
}

const std::vector<double>& Path::getWaitTimes() const {
	ROS_ASSERT(isValidPath);
	return waitTimes;
}

const std::vector<double>& Path::getDepartureTimes() const {
	ROS_ASSERT(isValidPath);
	return departureTimes;
}

float Path::getDistance() const {
	ROS_ASSERT(isValidPath);
	return distance;
}

double Path::getDuration() const {
	ROS_ASSERT(isValidPath);
	return duration;
}

float Path::getBatteryConsumption() const {
	ROS_ASSERT(isValidPath);
	return batteryConsumption;
}

visualization_msgs::Marker Path::getVisualizationMsgLines() {
	ROS_ASSERT(isValidPath);
	visualization_msgs::Marker msg;
	msg.header.frame_id = "map";
	msg.header.stamp = ros::Time::now();
	msg.ns = "PathLines";
	msg.action = visualization_msgs::Marker::ADD;
	msg.pose.orientation.w = 1.0;

	msg.id = 1;
	msg.type = visualization_msgs::Marker::LINE_STRIP;
	msg.scale.x = 0.1;

	// Set colors
	ros::NodeHandle nh("~");
	double color_r;
	double color_g;
	double color_b;
	if(!nh.getParam("color_r", color_r)) {
		color_r = 30;
	}	
	if(!nh.getParam("color_g", color_g)) {
		color_g = 200;
	}
	if(!nh.getParam("color_b", color_b)) {
		color_b = 40;
	}
	
	msg.color.a = 0.6f;
	msg.color.r = color_r / 255;
	msg.color.g = color_g / 255;
	msg.color.b = color_b / 255;


	for(const Point& point : nodes) {
		geometry_msgs::Point p;
		p.x = point.x;
		p.y = point.y;
		p.z = 0.f;
		msg.points.push_back(p);
	}

	return msg;
}

double Path::getStartTimeOffset() const {
	ROS_ASSERT(isValidPath);
	return startTimeOffset;
}

RobotHardwareProfile* Path::getRobotHardwareProfile() const {
	ROS_ASSERT(isValidPath);
	return hardwareProfile;
}

OrientedPoint Path::getStart() {
	ROS_ASSERT(isValidPath);
	return start;
}

OrientedPoint Path::getEnd() {
	ROS_ASSERT(isValidPath);
	return end;
}

bool Path::isValid() const {
	return isValidPath;
}

bool operator ==(const Path& p1, const Path& p2){
	if(p1.getNodes().size() != p2.getNodes().size()) {
		return false;
	}

	for(unsigned int i = 0; i < p1.getNodes().size(); i++) {
		if(!(p1.getNodes().at(i) == p2.getNodes().at(i))) {
			return false;
		}
	}

	return true;
}
