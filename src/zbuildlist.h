#ifndef _ZBUILDLIST_H_
#define _ZBUILDLIST_H_

#include "constants.h"
#include "zmap.h"
#include "zsettings.h"

#include <vector>

using namespace std;

class buildlist_object
{
public:
	buildlist_object() { clear(); }
	buildlist_object(unsigned char ot_, unsigned char oid_) {ot = ot_; oid = oid_;}

	void clear()
	{
		ot = 0;
		oid = 0;
	}

	unsigned char ot, oid;
};

class ZBuildList
{
public:
	ZBuildList();

	void LoadDefaults();
	//#183: cranes are only offered on maps that have bridges (otherwise they have
	//no real use and just let you cheese the enemy fort). Rebuilds the list and,
	//when the map has no bridges, strips every crane entry. Call on each map load.
	void SetMapHasBridges(bool has_bridges);
	void SetZSettings(ZSettings *zsettings_);
	vector<buildlist_object> &GetBuildList(int building_type, int level);
	bool UnitInBuildList(int building_type, int level, unsigned char ot, unsigned char oid);
	bool GetFirstUnitInBuildList(int building_type, int level, unsigned char &ot, unsigned char &oid);

	int UnitBuildTime(unsigned char ot, unsigned char oid);
private:
	void ClearData();
	vector<buildlist_object> buildlist_data[MAX_BUILDING_TYPES][MAX_BUILDING_LEVELS];

	ZSettings *zsettings;
};

#endif
