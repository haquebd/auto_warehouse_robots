#ifndef PROTOTYPE_MAP_H
#define PROTOTYPE_MAP_H

#include <vector>

#include "auto_smart_factory/Tray.h"
#include "auto_smart_factory/WarehouseConfiguration.h"
#include "agent/path_planning/Rectangle.h"
#include "agent/path_planning/GridNode.h"
#include "agent/path_planning/ThetaStarMap.h"
#include "agent/path_planning/Path.h"
#include "agent/path_planning/OrientedPoint.h"
#include "agent/path_planning/RobotHardwareProfile.h"
#include "agent/path_planning/TimedLineOfSightResult.h"

class Map {
public:
	static double infiniteReservationTime;
	
private:
	// For visualisation messages
	static int visualisationId;
	
	float width;
	float height;
	float margin;
	auto_smart_factory::WarehouseConfiguration warehouseConfig;
	
	std::vector<Rectangle> obstacles;
	std::vector<Rectangle> reservations;
	ThetaStarMap thetaStarMap;
	RobotHardwareProfile* hardwareProfile;
	
	int ownerId;

public:
	Map(auto_smart_factory::WarehouseConfiguration warehouseConfig, std::vector<Rectangle> &obstacles, RobotHardwareProfile* hardwareProfile, int ownerId);
	~Map() = default;

	// Visualisation
	visualization_msgs::Marker getObstacleVisualization();
	visualization_msgs::Marker getInactiveReservationVisualization(int ownerId, visualization_msgs::Marker::_color_type baseColor);
	visualization_msgs::Marker getActiveReservationVisualization(int ownerId, visualization_msgs::Marker::_color_type baseColor);

	// Line of sight checks
	bool isInsideAnyStaticInflatedObstacle(const Point& point) const;	
	bool isStaticLineOfSightFree(const Point& pos1, const Point& pos2) const;

	TimedLineOfSightResult whenIsTimedLineOfSightFree(const Point& pos1, double startTime, const Point& pos2, double endTime, const std::vector<Rectangle>& smallerReservations) const;
	bool isTimedConnectionFree(const Point& pos1, const Point& pos2, double startTime, double waitingTime, double drivingTime, const std::vector<Rectangle>& smallerReservations) const;
	
	bool isPointInMap(const Point& pos) const;
	Point getRandomFreePoint() const;
	OrientedPoint getPointInFrontOfTray(const auto_smart_factory::Tray& tray);

	// Reservations
	void addReservations(const std::vector<Rectangle>& newReservations);
	void deleteExpiredReservations(double time);
	std::vector<Rectangle> deleteReservationsFromAgent(int agentId);
		
	// Path queries
	Path getThetaStarPath(const OrientedPoint& start, const OrientedPoint& end, double startingTime, double targetReservationTime, bool ignoreStartingReservations);
	Path getThetaStarPath(const OrientedPoint& start, const auto_smart_factory::Tray& end, double startingTime, double targetReservationTime);
	Path getThetaStarPath(const auto_smart_factory::Tray& start, const OrientedPoint& end, double startingTime, double targetReservationTime);
	Path getThetaStarPath(const auto_smart_factory::Tray& start, const auto_smart_factory::Tray& end, double startingTime, double targetReservationTime);
	
	bool isPointTargetOfAnotherRobot(OrientedPoint pos);
	bool isPointTargetOfAnotherRobot(const auto_smart_factory::Tray& tray);
	
	// Getter
	float getWidth() const;
	float getHeight() const;
	float getMargin() const;
	int getOwnerId() const;
	
	void listAllReservationsIn(Point p);
	std::vector<Rectangle> getRectanglesOnStartingPoint(Point p) const;

private:

};


#endif //PROTOTYPE_MAP_H
