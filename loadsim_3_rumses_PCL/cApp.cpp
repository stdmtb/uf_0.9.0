//#define RENDER

#include "cinder/app/App.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/Perlin.h"
#include "cinder/params/Params.h"
#include "cinder/Xml.h"

#include "ConsoleColor.h"
#include "Exporter.h"
#include "Ramses.h"
#include "mtUtil.h"
#include "VboSet.h"

#include <pcl/search/kdtree.h> 
#include <pcl/search/organized.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>

using namespace ci;
using namespace ci::app;
using namespace std;

class cApp : public App {
    
public:
    void setup();
    void update();
    void draw();
    void mouseDown( MouseEvent event );
    void mouseDrag( MouseEvent event );
    void keyDown( KeyEvent event );
    void resize();
    
    void makeGui();
    void saveXml();
    void loadXml();
    
    void feature3d( const vector<vec3> & pos );
    
    CameraUi camUi;
    Perlin mPln;
    Exporter mExp;
    
    vector<Ramses> rms;
    bool bStart = false;
    bool bOrtho = false;
    bool bFall = false;

    int eSimType = 0;
    int frame = 100;

    params::InterfaceGlRef gui;
    CameraPersp cam;
    
    int resolution = 1;
    
    VboSet norms;
};

void cApp::feature3d( const vector<vec3> & pos ){
    
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
    
    for( int i=0; i<pos.size(); i++){
        
        pcl::PointXYZ point;
        point.x = pos[i].x;
        point.y = pos[i].y;
        point.z = pos[i].z;
        cloud->points.push_back( point );
    }
        
    // Create the normal estimation class, and pass the input dataset to it
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud (cloud);
    
    // Create an empty kdtree representation, and pass it to the normal estimation object.
    // Its content will be filled inside the object, based on the given input dataset (as no other search surface is given).
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ> ());
    ne.setSearchMethod(tree);
    
    // Output datasets
    pcl::PointCloud<pcl::Normal>::Ptr cloud_normals (new pcl::PointCloud<pcl::Normal>);
    
    // Use all neighbors in a sphere of radius 3cm
    ne.setRadiusSearch (0.03);
    ne.compute (*cloud_normals);

    const vector<pcl::Normal, Eigen::aligned_allocator<pcl::Normal>> &n = cloud_normals->points;

    cout << pos.size() << "  " << n.size() << endl;

    for(int i=0; i<n.size(); i++){
        float x = n[i].normal_x;
        float y = n[i].normal_y;
        float z = n[i].normal_z;
        
        norms.addPos( pos );
        norms.addPos( vec3(x,y,z) );
        norms.addCol(ColorAf(x,y,z,1));
        norms.addCol(ColorAf(x,y,z,1));
    }
}

void cApp::setup(){
    setWindowPos( 0, 0 );
    
    float w = 1920;
    float h = 1080*3;
    
    setWindowSize( w*0.2, h*0.2 );
    mExp.setup( w, h, 0, 550-1, GL_RGB, mt::getRenderPath(), 0);
    
    cam = CameraPersp(w, h, 55.0f, 0.1, 1000000 );
    
    if(0){
        cam.lookAt( vec3(0,0,800), vec3(0,0,0) );
        cam.setLensShift( 0,0 );
    }else{
        cam.setNearClip(0.100000);
        cam.setFarClip(1000000.000000);
        cam.setAspectRatio(0.592593);
        cam.setFov(55.000000);
        cam.setEyePoint(vec3(326.924622,-381.081604,259.431519));
        cam.setWorldUp(vec3(0.000000,1.000000,0.000000));
        cam.setLensShift(vec2(0.000000,0.000000));
        cam.setViewDirection(vec3(-0.578462,0.674288,-0.459040));
        cam.lookAt(vec3(326.924622,-381.081604,259.431519)+vec3(-0.578462,0.674288,-0.459040));
    }
    
    camUi.setCamera( &cam );
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    for( int i=0; i<6; i++){
        Ramses r(eSimType,i);
        rms.push_back( r );
    }

    makeGui();
    
#ifdef RENDER
    mExp.startRender();
#endif
    
}

void cApp::update(){

    if( !bStart ) return;
    
    if( bFall ){
        
        // move particle up/down
        for( int i=0; i<rms.size(); i++){
            Ramses & rm = rms[i];
            rm.move();
        }
    
    }else{
        for( int i=0; i<rms.size(); i++){
            if( rms[i].bShow ){
                rms[i].loadSimData( frame );
                rms[i].updateVbo(resolution);
            }
        }
    }
}

void cApp::draw(){
    
    bOrtho ? mExp.beginOrtho( true ) : mExp.begin( camUi.getCamera() ); {
        
        gl::clear();    
        gl::enableDepthRead();
        gl::enableDepthWrite();
        gl::enableAlphaBlending();
        glPointSize(1);
        glLineWidth(1);
    
        if( !mExp.bRender && !mExp.bSnap ){ mt::drawCoordinate(10); }
        for( int i=0; i<rms.size(); i++){
            rms[rms.size()-i-1].draw();
        }
        
        norms.draw();
        
    }mExp.end();
    
    mExp.draw();
    
    if(gui) gui->draw();

    if( bStart && !bFall )frame++;
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 'S': mExp.startRender();  break;
        case 'T': mExp.stopRender();  break;
        case 's': mExp.snapShot();  break;
        case ' ': bStart = !bStart; break;
        case 'c': mt::printCamera( camUi.getCamera() ); break;
    }
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
//    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
//    cam.setAspectRatio( getWindowAspectRatio() );
//    camUi.setCamera( &cam );
}

void cApp::makeGui(){
    gui = params::InterfaceGl::create( getWindow(), Ramses::simType[eSimType], vec2(300, getWindowHeight()) );
    gui->setOptions( "", "position=`0 0` valueswidth=100" );
    
    function<void(void)> update = [&](){
        for( int i=0; i<rms.size(); i++){ rms[i].updateVbo(resolution); }
    };
    
    function<void(void)> changeSym = [this](){
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = eSimType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo(resolution);
        }
    };
    
    function<void(void)> sx = [this](){
        saveXml();
    };
    
    function<void(void)> ld = [this](){
        loadXml();
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = eSimType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo(resolution);
        }
    };
    
    function<void(void)> ren = [this](){
        bStart = true;
        mExp.startRender();
    };
    
    function<void(void)> norm = [this](){

        norms.resetCol();
        norms.resetPos();
        norms.resetVbo();
        
        for( int i=0; i<rms.size(); i++){
            if(rms[i].bShow){
                feature3d( rms[i].pos );
            }
        }
        norms.init(GL_LINES);
    };
    
    gui->addText( "main" );
    gui->addParam("simType", &eSimType ).min(0).max(4).updateFn( changeSym );
    gui->addParam("start", &bStart );
    gui->addParam("frame", &frame ).updateFn(update);
    gui->addParam("ortho", &bOrtho );
    gui->addButton("Compute Norm", norm );

    gui->addParam("xyz global scale", &Ramses::globalScale ).step(0.01).updateFn(update);
    gui->addParam("r(x) resolution", &Ramses::boxelx, true );
    gui->addParam("theta(y) resolution", &Ramses::boxely, true );
    gui->addButton("save XML", sx );
    gui->addButton("load XML", ld );
    gui->addButton("start render", ren );
    
    gui->addSeparator();
    
    for( int i=0; i<6; i++){
        string p = Ramses::prm[i];
        //gui->addText( p );
        
        //function<void(void)> up = bind(&Ramses::updateVbo, &rms[i]);
        function<void(void)> up = [i, this](){
            rms[i].updateVbo(resolution);
        };
        
        function<void(void)> up2 = [i, this](){
            rms[i].loadSimData(this->frame);
            rms[i].updateVbo(resolution);
        };
        
        gui->addParam(p+" show", &rms[i].bShow ).group(p).updateFn(up2);
        gui->addParam(p+" polar coordinate", &rms[i].bPolar ).group(p).updateFn(up2);
        gui->addParam(p+" Auto Min Max", &rms[i].bAutoMinMax ).group(p).updateFn(up);
        gui->addParam(p+" in min", &rms[i].in_min).step(0.05f).group(p).updateFn(up);
        gui->addParam(p+" in max", &rms[i].in_max).step(0.05f).group(p).updateFn(up);
        
        gui->addParam(p+" z extrude", &rms[i].extrude).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" x offset", &rms[i].xoffset).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" y offset", &rms[i].yoffset).step(1.0f).group(p).updateFn(up);
        gui->addParam(p+" z offset", &rms[i].zoffset).step(1.0f).group(p).updateFn(up);
        
        gui->addParam(p+" xy scale", &rms[i].scale).step(1.0f).group(p).updateFn(up);
        //gui->addParam(p+" visible thresh", &rms[i].visible_thresh).step(0.005f).min(0.0f).max(1.0f).group(p).updateFn(up);
        gui->addParam(p+" log", &rms[i].eStretch).step(1).min(0).max(1).group(p).updateFn(up2);
        gui->addParam(p+" inAngle", &rms[i].inAngle).step(1).min(-180).max(180).group(p).updateFn(up);
        gui->addParam(p+" outAngle", &rms[i].outAngle).step(1).min(-180).max(180).group(p).updateFn(up);
        gui->addParam(p+" offsetRotateAngle", &rms[i].offsetRotateAngle).step(0.01).group(p).updateFn(up);
        gui->addParam(p+" rotateSpeed", &rms[i].rotateSpeed).step(0.01).group(p).updateFn(up);
        
        // read only
        //gui->addParam(p+" visible rate(%)", &rms[i].visible_rate, true ).group(p);
        //gui->addParam(p+" num particle", &rms[i].nParticle, true).group(p);
        
        gui->addSeparator();
    }
}

void cApp::loadXml(){
    
    fs::path p = "gui.xxml";
    if( !fs::is_empty( p ) ){
        XmlTree xml( loadFile( p ) );
        XmlTree mn = xml.getChild("gui_setting/main");
        eSimType =  (mn/"simType").getValue<int>();
        //frame =  (mn/"frame").getValue<int>();
        bOrtho =  (mn/"ortho").getValue<bool>();
        Ramses::globalScale =  (mn/"xyz_global_scale").getValue<float>();
        Ramses::boxelx =  (mn/"r_resolution").getValue<float>();
        Ramses::boxely =  (mn/"theta_resolution").getValue<float>();
        
        for( int i=0; i<rms.size(); i++){
            Ramses & r = rms[i];
            string name = Ramses::prm[i];
            XmlTree prm;
            prm = xml.getChild("gui_setting/"+name);
            r.bShow = (prm/"show").getValue<bool>();
            r.bPolar = (prm/"polar").getValue<bool>(true);
            r.bAutoMinMax = (prm/"Auto_Min_Max").getValue<bool>();
            r.in_min = (prm/"in_min").getValue<float>();
            r.in_max = (prm/"in_max").getValue<float>();
            r.extrude = (prm/"z_extrude").getValue<float>();
            r.xoffset = (prm/"x_offset").getValue<float>();
            r.yoffset = (prm/"y_offset").getValue<float>();
            r.zoffset = (prm/"z_offset").getValue<float>();
            r.scale = (prm/"xy_scale").getValue<float>();
            r.eStretch = (prm/"log").getValue<float>();
            r.inAngle = (prm/"inAngle").getValue<float>();
            r.outAngle = (prm/"outAngle").getValue<float>();
            r.offsetRotateAngle = (prm/"offsetRotateAngle").getValue<float>();
            r.rotateSpeed = (prm/"rotateSpeed").getValue<float>();
        }
    }
}

void cApp::saveXml(){
    
    XmlTree xml, mn;
    xml.setTag("gui_setting");
    mn.setTag("main");
    
    mn.push_back( XmlTree("simType", to_string(eSimType)) );
    //mn.push_back( XmlTree("frame", to_string(frame) ));
    mn.push_back( XmlTree("ortho", to_string(bOrtho) ));
    mn.push_back( XmlTree("xyz_global_scale", to_string(Ramses::globalScale) ));
    mn.push_back( XmlTree("r_resolution", to_string(Ramses::boxelx) ));
    mn.push_back( XmlTree("theta_resolution", to_string(Ramses::boxely) ));
    
    xml.push_back( mn );
    
    for( int i=0; i<rms.size(); i++){
        Ramses & r = rms[i];
        string name = Ramses::prm[i];
        XmlTree prm;
        prm.setTag(name);
        prm.push_back( XmlTree("show", to_string( r.bShow) ));
        prm.push_back( XmlTree("polar", to_string( r.bPolar )));
        prm.push_back( XmlTree("Auto_Min_Max", to_string( r.bAutoMinMax) ));
        prm.push_back( XmlTree("in_min", to_string(r.in_min) ));
        prm.push_back( XmlTree("in_max",to_string(r.in_max)));
        prm.push_back( XmlTree("z_extrude", to_string(r.extrude)));
        prm.push_back( XmlTree("x_offset", to_string(r.xoffset)));
        prm.push_back( XmlTree("y_offset", to_string(r.yoffset)));
        prm.push_back( XmlTree("z_offset", to_string(r.zoffset)));
        prm.push_back( XmlTree("xy_scale", to_string( r.scale )));
        prm.push_back( XmlTree("log", to_string( r.eStretch )));
        prm.push_back( XmlTree("inAngle", to_string(r.inAngle)));
        prm.push_back( XmlTree("outAngle", to_string(r.outAngle)));
        prm.push_back( XmlTree("offsetRotateAngle", to_string(r.offsetRotateAngle)));
        prm.push_back( XmlTree("rotateSpeed", to_string(r.rotateSpeed)));
        xml.push_back( prm );
    }
    
    DataTargetRef file = DataTargetPath::createRef( "gui.xxml" );
    xml.write( file );
    
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
