#ifndef PROTOTYPE_RECTANGLE_HPP
#define PROTOTYPE_RECTANGLE_HPP

#include "agent/path_planning/Point.h"

class Rectangle {
private:	
	Point pos;
	Point size;
	float rotation;

	double startTime;
	double endTime;
	
	int ownerId;
	
	// Faster physic processing
	Point pointsInflated[4];
	Point pointsNonInflated[4];
	bool isAxisAligned;
	double minXInflated, maxXInflated, minYInflated, maxYInflated;

public:
	Rectangle(Point pos, Point size, float rotation);
	Rectangle(Point pos, Point size, float rotation, double startTime, double endTime, int ownerId);
	
	const Point* getPointsInflated() const;
	const Point* getPointsNonInflated() const;
	Point getPosition() const;
	Point getSize() const;
	float getRotation() const;
	
	bool getIsAxisAligned() const;
	double getMinXInflated() const;
	double getMaxXInflated() const;
	double getMinYInflated() const;
	double getMaxYInflated() const;

	bool doesOverlapTimeRange(double start, double end, int ownerId) const;
	double getStartTime() const;
	double getEndTime() const;
	double getFreeAfter() const;
	
	int getOwnerId() const;

};

bool operator ==(const Rectangle& left, const Rectangle& right);
bool operator !=(const Rectangle& left, const Rectangle& right);


#endif //PROTOTYPE_RECTANGLE_HPP
