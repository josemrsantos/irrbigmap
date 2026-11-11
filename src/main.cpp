#include <irrlicht.h>
#include "driverChoice.h"
#include <iostream>
#include <string>
#include <sstream>
#include "TiledTerrain.h"
#include "OverviewTerrain.h"

using namespace irr;
using namespace std;

// Size of the included map, in tiles (files heightmap_r1_c1 .. heightmap_r93_c140)
static const int MAP_ROWS = 93;
static const int MAP_COLUMNS = 140;

// The overview is drawn in its own pass with a much longer view distance than
// the detailed tiles (1 unit = 1 dm: 40,000,000 units = 4,000 km).
static const f32 DETAIL_NEAR = 1.0f;
static const f32 DETAIL_FAR = 5000000.0f;
static const f32 OVERVIEW_NEAR = 1000.0f;
static const f32 OVERVIEW_FAR = 40000000.0f;

#ifdef _MSC_VER
#pragma comment(lib, "Irrlicht.lib")
#endif

/********************** CLASS MyEventReceiver **********************/ 
class MyEventReceiver : public IEventReceiver
{
public:

  MyEventReceiver(TiledTerrain* terrain, scene::ISceneNode* skydome, scene::ICameraSceneNode* camera, scene::ISceneManager* smgr )
  {
    this->Terrain = terrain; 
    this->Skydome=skydome; 
    this->Camera=camera; 
    this->Smgr=smgr;
    this->showDebug = false;
    this->overviewOn = true;
    Skydome->setVisible(true);
  }

  bool overviewOn; // draw the distant whole-map overview (key O)

  void redoCameraHeight(f32 height)
  {
    Camera->setPosition(core::vector3df(Camera->getAbsolutePosition().X,                                                                                                                 
					Terrain->getCurrentTerrain()->getHeight(Camera->getAbsolutePosition().X,Camera->getAbsolutePosition().Z)+height,
					Camera->getAbsolutePosition().Z));
  }

  void redoCameraSpeed(f32 speed)
  {
    core::list<irr::scene::ISceneNodeAnimator*>::ConstIterator anims=Camera->getAnimators().begin();
    irr::scene::ISceneNodeAnimatorCameraFPS *anim=(irr::scene::ISceneNodeAnimatorCameraFPS*)*anims;
    anim->setMoveSpeed(speed);
  }
  
  bool OnEvent(const SEvent& event)
  {
    // check if user presses the key 'W' or 'D'
    if (event.EventType == irr::EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown)
      {
	switch (event.KeyInput.Key)
	  {
	  case irr::KEY_KEY_W: // switch wire frame mode
	    Terrain->getCurrentTerrain()->setMaterialFlag(video::EMF_WIREFRAME,
							  !Terrain->getCurrentTerrain()->getMaterial(0).Wireframe);
	    Terrain->getCurrentTerrain()->setMaterialFlag(video::EMF_POINTCLOUD, false);
	    return true;
	  case irr::KEY_KEY_P: // switch wire frame mode
	    Terrain->getCurrentTerrain()->setMaterialFlag(video::EMF_POINTCLOUD,
							  !Terrain->getCurrentTerrain()->getMaterial(0).PointCloud);
	    Terrain->getCurrentTerrain()->setMaterialFlag(video::EMF_WIREFRAME, false);
	    return true;
	  case irr::KEY_KEY_D: // toggle detail map
	    Terrain->getCurrentTerrain()->setMaterialType(
							  Terrain->getCurrentTerrain()->getMaterial(0).MaterialType == video::EMT_SOLID ?
				     video::EMT_DETAIL_MAP : video::EMT_SOLID);
	    return true;
	  case irr::KEY_KEY_X: // toggle debug information
	    showDebug=!showDebug;
	    Terrain->getCurrentTerrain()->setDebugDataVisible(showDebug?scene::EDS_BBOX_ALL:scene::EDS_OFF);
	    return true;
	  case irr::KEY_KEY_1: // move camera 2m above terrain
	    redoCameraHeight(20);
	    return true;
	  case irr::KEY_KEY_2: // move camera 10m above terrain
	    redoCameraHeight(100);
	    return true;
	  case irr::KEY_KEY_3: // move camera 5.000feet above terrain
	    redoCameraHeight(15240);
	    return true;
	  case irr::KEY_KEY_4: // move camera 20.000feet above terrain
	    redoCameraHeight(60960);
	    return true;
	  case irr::KEY_KEY_5: // move camera 40.000feet above terrain
	    redoCameraHeight(121920);
	    return true;
	  case irr::KEY_KEY_0: // move camera 300km above terrain (to see the overview)
	    redoCameraHeight(3000000);
	    return true;
	  case irr::KEY_KEY_O: // toggle the distant overview
	    overviewOn = !overviewOn;
	    return true;
	  case irr::KEY_KEY_6: // Camera speed = 0.1
	    redoCameraSpeed(0.1f);
	    return true;
	  case irr::KEY_KEY_7: // Camera speed = 1
	    redoCameraSpeed(1.0f);
	    return true;
	  case irr::KEY_KEY_8: // Camera speed = 20 
	    redoCameraSpeed(20.0f);
	    return true;
	  case irr::KEY_KEY_9: // Camera speed = 50
	    redoCameraSpeed(50.0f);
	    return true;
	  case irr::KEY_KEY_Q: // quit
	    exit(0);
	  default:
	    break;
	  }
      }    
    return false;
  }

private:
  TiledTerrain* Terrain;
  scene::ICameraSceneNode* Camera;
  scene::ISceneNode* Skydome;
  bool showDebug;
  scene::ISceneManager* Smgr;
};


// Show or hide everything that belongs to the detailed tiles (all top-level scene
// nodes except the camera, the sky and the overview), so that the overview and the
// detailed tiles can be drawn in separate passes.
static void setDetailVisible(scene::ISceneManager* smgr,
			     scene::ISceneNode* camera,
			     scene::ISceneNode* skydome,
			     scene::ISceneNode* overview,
			     bool visible)
{
  const core::list<scene::ISceneNode*>& children = smgr->getRootSceneNode()->getChildren();
  for (core::list<scene::ISceneNode*>::ConstIterator it = children.begin(); it != children.end(); ++it)
    {
      scene::ISceneNode* node = *it;
      if (node == camera || node == skydome || node == overview)
	continue;
      node->setVisible(visible);
    }
}

/********************** Main **********************/
int main()
{
  int num_layers = 2;
  f32 tile_size = 128.f*900.f;

  // ask user for driver
  video::E_DRIVER_TYPE driverType=driverChoiceConsole();
  if (driverType==video::EDT_COUNT)
    return 1;

  // device
  irr::SIrrlichtCreationParameters params;
  params.DriverType=driverType;
  params.WindowSize=core::dimension2d<u32>(800, 600);
  params.Fullscreen= false;
  //  IrrlichtDevice *nulldevice = createDevice(video::EDT_NULL);// create a NULL device to detect screen resolution
  //  params.WindowSize=nulldevice->getVideoModeList()->getDesktopResolution();
  //  params.Fullscreen= true;
  IrrlichtDevice* device = createDeviceEx(params);
  if (device == 0)
    return 1;
  video::IVideoDriver* driver = device->getVideoDriver();
  scene::ISceneManager* smgr = device->getSceneManager();
  gui::IGUIEnvironment* env = device->getGUIEnvironment();
  device->getCursorControl()->setVisible(false);  // disable mouse cursor

  // Fog
  driver->setFog(video::SColor(0,150,150,160), irr::video::EFT_FOG_LINEAR, 150000, 200000, 0.6f, false, false);

  // Camera
  scene::ICameraSceneNode* camera = smgr->addCameraSceneNodeFPS(0,250.0f,20.0f);
  camera->setPosition(core::vector3df((-1)*tile_size*(num_layers-1)-57000,3000,tile_size*(num_layers)+57000)); // Camera set at center of terrain
  camera->setTarget(core::vector3df(0,0,0));
  camera->setFarValue(DETAIL_FAR);
  camera->setNearValue(DETAIL_NEAR);

  // Light
//  smgr->setAmbientLight(video::SColor(0,60,60,60));
//  smgr->setAmbientLight(video::SColor(0,255,255,255));

  // skydome
  scene::ISceneNode* skydome=smgr->addSkyDomeSceneNode(driver->getTexture("media/skydome.jpg"),16,8,0.85f,2.0f);

  // Terrain
  TiledTerrain terrain(MAP_ROWS, //MaxRow,
		       MAP_COLUMNS, //MaxColumn,
		       num_layers, //num_layers
		       device,
		       smgr,
		       driver,
		       40, //initial_row,
		       62, //initial_column,
		       tile_size,
		       900.f, //scale XZ
		       180.f, //scale Y 
		       10000.f //margin = 1km 
		       ); 			   
  // scale=900.f : heightmap has resolution of 90m. with a scale of 900, each irrlicht unit will represent 1dm
  // scale=180.f : The highest point on the heightmap is 4590m and we have 255 colors available. 
  // Each color represents jump of 18m = 180dm = 180.f irrlicht units.  

  // Distant overview of the whole map (made by tools/make_overview.py).
  // 900.f/180.f are the same scales as above; tile_size/scaleY are all it needs.
  OverviewTerrain overview(smgr, driver,
			   "media/overview_height.png",
			   "media/overview_color.jpg",
			   MAP_ROWS, MAP_COLUMNS, tile_size, 180.f);

  // event receiver
  MyEventReceiver receiver(&terrain, skydome, camera, smgr);
  device->setEventReceiver(&receiver);
  
  /*
  driver->beginScene(true, true, video::SColor(0,0,50,0) );
  smgr->drawAll();
  env->drawAll();
  driver->endScene();
  */
  int lastFPS = -1;
  while(device->run())
    if (device->isWindowActive())
      {
	driver->beginScene(true, true, video::SColor(0,0,50,0) );
	if (receiver.overviewOn && overview.isValid())
	  {
	    // Pass 1: sky + distant overview, with a very long view distance.
	    overview.setCenter(terrain.getCurrentRow(), terrain.getCurrentColumn(), terrain.getNumLayers());
	    setDetailVisible(smgr, camera, skydome, overview.getNode(), false);
	    overview.getNode()->setVisible(true);
	    skydome->setVisible(true);
	    camera->setNearValue(OVERVIEW_NEAR);
	    camera->setFarValue(OVERVIEW_FAR);
	    smgr->drawAll();

	    // Pass 2: detailed tiles on top, with their own (fine) depth range.
	    driver->clearZBuffer();
	    overview.getNode()->setVisible(false);
	    skydome->setVisible(false);
	    setDetailVisible(smgr, camera, skydome, overview.getNode(), true);
	    camera->setNearValue(DETAIL_NEAR);
	    camera->setFarValue(DETAIL_FAR);
	    smgr->drawAll();
	  }
	else
	  {
	    if (overview.isValid())
	      overview.getNode()->setVisible(false);
	    skydome->setVisible(true);
	    smgr->drawAll();
	  }
	env->drawAll();
	driver->endScene();

	terrain.shiftTerrain(camera);
	// altitude
	int fps = driver->getFPS();
	if (lastFPS != fps || true)
	  {
	    core::stringw str = L"[";
	    str += "FPS:";
	    str += fps;
	    str += "] X: ";
	    str += camera->getAbsolutePosition().X;
	    str += " Y: ";
	    str += camera->getAbsolutePosition().Y;
	    str += " Z: ";
	    str += camera->getAbsolutePosition().Z;
	    device->setWindowCaption(str.c_str());
	    lastFPS = fps;
	  }
      }
  device->drop();	
  return 0;
}
