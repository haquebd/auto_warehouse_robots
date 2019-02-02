#include <queue>
#include <include/agent/path_planning/TimedLineOfSightResult.h>

#include "ros/ros.h"
#include "Math.h"
#include "agent/path_planning/ThetaStarPathPlanner.h"

ThetaStarPathPlanner::ThetaStarPathPlanner(ThetaStarMap* thetaStarMap, RobotHardwareProfile* hardwareProfile) :
	map(thetaStarMap),
	hardwareProfile(hardwareProfile) 
{	
}

Path ThetaStarPathPlanner::findPath(OrientedPoint start, OrientedPoint target, double startingTime) {
	startPoint = start;
	endPoint = target;

	map->addAdditionalNode(Point(start.x, target.y));
	map->addAdditionalNode(Point(target.x, target.y));
	
	const GridNode* startNode = map->getNodeClosestTo(Point(start));
	const GridNode* targetNode = map->getNodeClosestTo(Point(target));
	
	if(startNode == nullptr || targetNode == nullptr) {
		if(startNode == nullptr) {
			ROS_FATAL("StartPoint %f/%f is no in theta* map!", start.x, start.y);
		}
		if(targetNode == nullptr) {
			ROS_FATAL("TargetPoint %f/%f is no in theta* map!", target.x, target.y);
		}
		exit(1);
		return Path();
	}
	
	double initialWaitTime = 0;
	TimedLineOfSightResult initialCheckResult = map->whenIsTimedLineOfSightFree(Point(start.x, start.y), startingTime, startNode->pos, startingTime + 0.5f);
	if(initialCheckResult.blockedByTimed) {
		initialWaitTime = initialCheckResult.freeAfter - startingTime + 0.5f;
		ROS_WARN("Path needed initial wait time of %f", initialWaitTime);
	}

	GridInformationMap exploredSet;
	GridInformationPairQueue queue;

	// Push start node
	exploredSet.insert(std::make_pair(startNode->pos, ThetaStarGridNodeInformation(startNode, nullptr, startingTime + initialWaitTime)));
	queue.push(std::make_pair(startingTime + initialWaitTime, &exploredSet.at(startNode->pos)));

	bool targetFound = false;
	ThetaStarGridNodeInformation* targetInformation = nullptr;

	while(!queue.empty()) {
		ThetaStarGridNodeInformation* current = queue.top().second;
		ThetaStarGridNodeInformation* prev = current->prev;
		queue.pop();

		// Target found
		if(current->node == targetNode) {
			targetFound = true;
			targetInformation = current;
			break;
		}

		// Explore all neighbours		
		for(auto neighbourNode : current->node->neighbours) {
			ThetaStarGridNodeInformation* neighbour = &exploredSet.insert(std::make_pair(neighbourNode->pos, ThetaStarGridNodeInformation(neighbourNode, nullptr, initialTime))).first->second;

			// Driving time only includes the additional time to drive from newPrev to neighbour.
			// Therefore: newPrev->time + drivingTime + waitingTime = neighbour->time must be true!
			// Waiting time is the time which must be waited at newPrev before driving can start.

			// Turning time is considered as part of the following line segment. => drivingTime includes previous turning time
			// Therefore, a line segment driving time = Time to turn to target Point + driving time to target point

			double drivingTime = 0;
			double waitingTime = 0;
			ThetaStarGridNodeInformation* newPrev = nullptr;
			bool makeConnection = false;

			// Only try direct connection with prev if not at start node
			bool connectionWithPrevPossible = prev != nullptr;
			if(connectionWithPrevPossible) {
				double timeAtPrev = prev->time;
				double timeAtNeighbour = prev->time + getDrivingTime(prev, neighbour);

				TimedLineOfSightResult result = map->whenIsTimedLineOfSightFree(prev->node->pos, timeAtPrev, neighbour->node->pos, timeAtNeighbour);
				connectionWithPrevPossible = !result.blockedByStatic && !result.blockedByTimed && (!result.hasUpcomingObstacle || (result.hasUpcomingObstacle && timeAtNeighbour < result.lastValidEntryTime));
			}

			if(connectionWithPrevPossible) {
				drivingTime = getDrivingTime(prev, neighbour);
				newPrev = prev;
				makeConnection = true;
			} else {
				double timeAtCurrent = current->time;
				double timeAtNeighbour = current->time + getDrivingTime(current, neighbour);
				TimedLineOfSightResult result = map->whenIsTimedLineOfSightFree(current->node->pos, timeAtCurrent, neighbour->node->pos, timeAtNeighbour);

				if(!result.blockedByStatic) {
					bool waitBecauseUpcomingObstacle = result.hasUpcomingObstacle && timeAtNeighbour >= result.lastValidEntryTime;

					if(!result.blockedByTimed && !waitBecauseUpcomingObstacle) {
						drivingTime = getDrivingTime(current, neighbour);
						newPrev = current;
						makeConnection = true;
					} else {
						// Wait
						if(waitBecauseUpcomingObstacle) {
							waitingTime = result.freeAfterUpcomingObstacle - current->time;
							//printf("Waiting because of upcoming obstacle: %f - Pos: %.1f/%.1f\n", waitingTime, current->node->pos.x, current->node->pos.y);
						} else {
							waitingTime = result.freeAfter - current->time;
							//printf("Waiting because of current obstacle: %f - Pos: %.1f/%.1f - CurrentTime: %f\n", waitingTime, current->node->pos.x, current->node->pos.y, current->time);
						}

						drivingTime = getDrivingTime(current, neighbour);
						newPrev = current;
						makeConnection = true;
					}
				}
			}

			if(makeConnection && (newPrev->time + drivingTime + waitingTime) < neighbour->time) {
				// Check for if connection is valid for upcoming obstacles

				if(map->isTimedConnectionFree(newPrev->node->pos, neighbour->node->pos, newPrev->time, waitingTime, drivingTime)) {
					double heuristic = getHeuristic(neighbour, targetNode->pos);

					neighbour->time = newPrev->time + drivingTime + waitingTime;
					neighbour->prev = newPrev;
					neighbour->waitTimeAtPrev = waitingTime;
					queue.push(std::make_pair(neighbour->time + heuristic, neighbour));
				}
			}
		}
	}

	if(targetFound) {
		return constructPath(startingTime, targetInformation, start, target, initialWaitTime);
	} else {
		ROS_FATAL("No path found from node %f/%f to node %f/%f!", startNode->pos.x,startNode->pos.y, targetNode->pos.x, targetNode->pos.y);
		map->listAllReservationsIn(targetNode->pos);
		
		exit(1);
		return Path();
	}
}

double ThetaStarPathPlanner::getHeuristic(ThetaStarGridNodeInformation* current, Point targetPos) const {
	return hardwareProfile->getDrivingDuration(Math::getDistance(current->node->pos, targetPos));
}

double ThetaStarPathPlanner::getDrivingTime(ThetaStarGridNodeInformation* current, ThetaStarGridNodeInformation* target) const {
	// Include turningTime to current line segment if prev is available
	double turningTime = 0;

	float prevLineSegmentRotation = 0;
	if(current->prev != nullptr) {
		prevLineSegmentRotation = Math::getRotation(current->node->pos - current->prev->node->pos);		
	} else {
		prevLineSegmentRotation = startPoint.o;
	}

	float currLineSegmentRotation = Math::getRotation(target->node->pos - current->node->pos);
	turningTime = hardwareProfile->getTurningDuration(std::abs(prevLineSegmentRotation - currLineSegmentRotation));

	return hardwareProfile->getDrivingDuration(Math::getDistance(current->node->pos, target->node->pos)) + turningTime;
}

Path ThetaStarPathPlanner::constructPath(double startingTime, ThetaStarGridNodeInformation* targetInformation, OrientedPoint start, OrientedPoint target, double initialWaitTime) const {
	std::vector<Point> pathNodes;
	std::vector<double> waitTimes;
	ThetaStarGridNodeInformation* currentGridInformation = targetInformation;
	double waitTimeAtPrev = 0;

	while(currentGridInformation != nullptr) {
		pathNodes.emplace_back(currentGridInformation->node->pos);
		waitTimes.push_back(waitTimeAtPrev);

		waitTimeAtPrev = currentGridInformation->waitTimeAtPrev;
		currentGridInformation = currentGridInformation->prev;
	}

	if(!(pathNodes.back() == Point(start))) {
		pathNodes.emplace_back(Point(start.x, start.y));
		waitTimes.push_back(initialWaitTime);
	} else {
		//waitTimes.front() = initialWaitTime;
	}
	
	std::reverse(pathNodes.begin(), pathNodes.end());
	std::reverse(waitTimes.begin(), waitTimes.end());

	return Path(startingTime, pathNodes, waitTimes, hardwareProfile);
}

