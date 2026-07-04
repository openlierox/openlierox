#ifndef GAME_H
#define GAME_H

#include "level.h"
#include "object_grid.h"
#include "FeatureList.h"
#include "OlxVariable.h"
#include "Consts.h" // for MAX_LOCAL_PLAYERS

#ifndef DEDICATED_ONLY
#include "gusanos/allegro.h"
#endif
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
#include "netstream.h"

class CWormInputHandler;
class CWorm;
class BaseAction;
struct PlayerOptions;
class WeaponType;
class Particle;
class PartType;
class Explosion;
class BitStream;
struct LuaEventDef;
#ifndef DEDICATED_ONLY
class Sound;
class Font;
#endif
class CMap;

class CWormHumanInputHandler;

struct Options
{
	void registerInConsole();
	
	OlxSpeedVar<FT_RopeSpeed> ninja_rope_shootSpeed;
	OlxRopeStrengthVar ninja_rope_pullForce;
	OlxVar<int,FT_RopeMaxLength> ninja_rope_startDistance;
	OlxVar<int,FT_RopeRestLength> ninja_rope_restLength;
	OlxSpeedVar<FT_WormMaxGroundMoveSpeed> worm_maxSpeed;
	OlxAccelVar<FT_WormAcceleration> worm_acceleration;
	OlxVar<float,FT_WormAirAccelerationFactor> worm_airAccelerationFactor;
	OlxWormFrictionVar<FT_WormGroundFriction> worm_friction;
	OlxWormFrictionVar<FT_WormSimpleFriction> worm_airFriction;
	OlxAccelVar<FT_WormGravity> worm_gravity;
	OlxVar<float,FT_WormBounceQuotient> worm_bounceQuotient;
	OlxVar<float,FT_WormBounceLimit> worm_bounceLimit;
	OlxNegatedSpeedVar<FT_WormJumpForce> worm_jumpForce;
	OlxBoolNegatedVar<FT_WormWallHugging> worm_disableWallHugging;
	OlxVar<int,FT_WormWeaponHeight> worm_weaponHeight;
	OlxVar<int,FT_WormHeight> worm_height;
	OlxVar<int,FT_WormWidth> worm_width;
	OlxVar<int,FT_WormMaxClimb> worm_maxClimb;
	OlxVar<float,FT_WormBoxRadius> worm_boxRadius;
	OlxVar<float,FT_WormBoxTop> worm_boxTop;
	OlxVar<float,FT_WormBoxBottom> worm_boxBottom;
	
	OlxTimeVar<FT_RespawnTime> minRespawnTime;
	OlxTimeVar<FT_MaxRespawnTime> maxRespawnTime;
	int maxWeaponsVar;
	size_t maxWeapons;
	bool showDeathMessages;
	bool logDeathMessages;
	
	int showMapDebug;
};

struct LevelEffectEvent
{
	LevelEffectEvent( int index_, int x_, int y_ ) : index(index_), x(x_), y(y_)
	{
	}
	int index;
	int x,y;
	
};

struct ScreenMessage
{
	enum Type
	{
		Death,
		Chat,
	};
	
	ScreenMessage(Type type_, std::string const& str_, int timeOut_ = 400)
	: type(type_), str(str_), timeOut(timeOut_)
	{
	}
	
	Type type;
	std::string str;
	int timeOut;
};

class GusGame
{
public:
	// Shared with the rest of OLX via Consts.h so the split-screen player count
	// and the gusanos local-input slots always match.
	static const size_t MAX_LOCAL_PLAYERS = ::MAX_LOCAL_PLAYERS;
	
	
		
	static Net_ClassID  classID;

	enum PLAYER_TYPE
	{
		OWNER = 0,
		PROXY,
	};
			
	GusGame();
	~GusGame();
	
	bool init();
	void parseCommandLine(int argc, char** argv);
	
	void think();
	
	bool setMod(const std::string& mod);
	void loadWeapons();
	void unload();
	bool isLoaded();
	void refreshResources(std::string const& levelPath);
	bool loadModWithoutMap();
	bool changeLevel(ResourceLocator<CMap>::BaseLoader* loader, const std::string& path, CMap* m = NULL);
	void runInitScripts();
	CWormInputHandler* findPlayerWithID( Net_NodeID ID );
	//static Net_Node* getNode();
	static void sendLuaEvent(LuaEventDef* event, eNet_SendMode mode, Net_U8 rules, BitStream* data, Net_ConnID connID);
	
	void assignNetworkRole();
	void removeNode();
	
	void applyLevelEffect( LevelEffect* effect, int x, int y );
	
	void displayChatMsg( std::string const& owner, std::string const& message);
	void displayKillMsg( CWormInputHandler* killed, CWormInputHandler* killer );
	void displayMessage( ScreenMessage const& msg );
	
	bool isEngineNeeded();
	
	std::vector<WeaponType*> weaponList;
	Options options;
	
	void insertExplosion( Explosion* explosion );
	
	std::map< std::string, BaseAction*(*)( const std::vector< std::string > & ) > actionList;
	//HashTable< std::string, BaseAction*(*)( const std::vector< std::string > & ) > actionList;
	
	PartType* NRPartType;
	PartType* deathObject;
	PartType* digObject;
		
	std::string const& getModPath();
	std::string const& getDefaultPath();

#ifndef DEDICATED_ONLY
	Sound* chatSound;
	Font *infoFont;
#endif
	std::list<ScreenMessage> messages;

	unsigned long stringToIndex(std::string const& str);
	
	std::string const& indexToString(unsigned long idx);
		
	static void addCRCs(BitStream* req);
	static bool checkCRCs(BitStream& data);
		
private:
	bool _loadMod();
	void _prepareLoad(const std::string& path);
	void _finishLoad();
};

extern GusGame gusGame;

#endif // _GAME_H_
