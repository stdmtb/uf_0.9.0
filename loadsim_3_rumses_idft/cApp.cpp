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
#include "SoundWriter.h"
#include "Dft.h"

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
    
    void proc_idft();
    void write_audio();
    
    const int outSamplingRate = 48000*4;
    
    CameraUi camUi;
    Perlin mPln;
    Exporter mExp;
    
    vector<Ramses> rms;
    bool bStart = false;
    bool bOrtho = false;
    int eSimType = 0;
    int frame = 100;
    
    bool bZsort = false;
    
    params::InterfaceGlRef gui;
    CameraPersp cam;
    
    vector<vector<float>> wavData;

};

void cApp::setup(){
    setWindowPos( 0, 0 );

    float w = 1920;
    float h = 1080;
    
    setWindowSize( w, h );
    mExp.setup( w, h, 0, 3000, GL_RGB, mt::getRenderPath(), 0);
    
    cam = CameraPersp(w, h, 55.0f, 1, 100000 );
    cam.lookAt( vec3(0,0,800), vec3(0,0,0) );
    cam.setLensShift( 0,0 );
    camUi.setCamera( &cam );
    
    mPln.setSeed(123);
    mPln.setOctaves(4);
    
    for( int i=0; i<6; i++){
        Ramses r(eSimType,i);
        rms.push_back( r );
    }

    makeGui();
    
    wavData.assign(6, vector<float>() );
    
#ifdef RENDER
    mExp.startRender();
#endif
    
}

void cApp::makeGui(){
    gui = params::InterfaceGl::create( getWindow(), Ramses::simType[eSimType], vec2(300, getWindowHeight()) );
    gui->setOptions( "", "position=`0 0` valueswidth=100" );
    
    function<void(void)> update = [&](){
        for( int i=0; i<rms.size(); i++){ rms[i].updateVbo(); }
    };
    
    function<void(void)> changeSym = [this](){
        for( int i=0; i<rms.size(); i++){
            rms[i].eSimType = eSimType;
            rms[i].loadSimData(frame);
            rms[i].updateVbo();
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
            rms[i].updateVbo();
        }
    };
    
    gui->addText( "main" );
    gui->addParam("simType", &eSimType ).min(0).max(4).updateFn( changeSym );
    gui->addParam("start", &bStart );
    gui->addParam("frame", &frame ).updateFn(update);
    gui->addParam("ortho", &bOrtho );
    gui->addParam("xyz global scale", &Ramses::globalScale ).step(0.01).updateFn(update);
    gui->addParam("r(x) resolution", &Ramses::boxelx, true );
    gui->addParam("theta(y) resolution", &Ramses::boxely, true );
    gui->addButton("save XML", sx );
    gui->addButton("load XML", ld );
    gui->addButton("Write Audio", [this](){ write_audio(); } );
    
    gui->addSeparator();
    
    for( int i=0; i<6; i++){
        string p = Ramses::prm[i];
        //gui->addText( p );
        
        //function<void(void)> up = bind(&Ramses::updateVbo, &rms[i]);
        function<void(void)> up = [i, this](){
            rms[i].updateVbo();
        };
        
        function<void(void)> up2 = [i, this](){
            rms[i].loadSimData(this->frame);
            rms[i].updateVbo();
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
        
        // read only
        gui->addParam(p+" visible rate(%)", &rms[i].visible_rate, true ).group(p);
        gui->addParam(p+" num particle", &rms[i].nParticle, true).group(p);
        
        gui->addSeparator();
    }
}

void cApp::loadXml(){
    
    fs::path p = "gui.xml";
    if( !fs::is_empty( p ) ){
        XmlTree xml( loadFile( p ) );
        XmlTree mn = xml.getChild("gui_setting/main");
        eSimType =  (mn/"simType").getValue<int>();
        frame =  (mn/"frame").getValue<int>();
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
            r.visible_rate = (prm/"visible_rate").getValue<float>();
            r.nParticle = (prm/"num_particle").getValue<int>();
        }
    }
}

void cApp::saveXml(){

    XmlTree xml, mn;
    xml.setTag("gui_setting");
    mn.setTag("main");
    
    mn.push_back( XmlTree("simType", to_string(eSimType)) );
    mn.push_back( XmlTree("frame", to_string(frame) ));
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
        prm.push_back( XmlTree("visible_rate", to_string( r.visible_rate )));
        prm.push_back( XmlTree("num_particle", to_string( r.nParticle )));
        xml.push_back( prm );
    }
    
    DataTargetRef file = DataTargetPath::createRef( "gui.xml" );
    xml.write( file );
    
}

void cApp::update(){
    if( bStart ){
        for( int i=0; i<rms.size(); i++){
            if( rms[i].bShow ){
                bool loadok = rms[i].loadSimData( frame );
                if( loadok ){
                    rms[i].updateVbo();
                }else{
                    write_audio();
                    exit(1);
                }                    
            }
        }
        
        proc_idft();
    }
}


void cApp::proc_idft(){
    
    fs::path dir = mt::getRenderPath();
    
    for( int i=0; i<rms.size(); i++){
        
        const Ramses & rm = rms[i];
        
        if( rm.bShow ){
            
            vector<vec3> pos = rm.pos;
            
            int dftSize = pos.size();
            dftSize = Dft::findBiggestDftSize(dftSize*2);
            
            if(1){
                audio::Buffer wave(dftSize);
                float * f = wave.getChannel(0);
                vector<float> freq = {4000.0f};
                
                for( int j=0; j<freq.size(); j++){
                    float stepRad = freq[j]*2.0f*pi / (float)outSamplingRate;
                    for( int i=0; i<wave.getSize(); i++){
                        float s = sin( i * stepRad ) * 0.1f;
                        f[i] += s;
                    }
                }
                Dft dft;
                audio::BufferSpectral spec = audio::BufferSpectral( dftSize );
                dft.forward(&wave, &spec, dftSize );

                audio::Buffer result_wave(dftSize);
                dft.inverse(&spec, &result_wave, dftSize);
                float * tmp = result_wave.getChannel(0);
                wavData[i].insert(wavData[i].end(), tmp, tmp+result_wave.getNumFrames()/2-1 );
            }
            
            else{
                //
                //  1. fill spectrum
                //
                audio::BufferSpectral spec(dftSize);
                float * real = spec.getReal();
                float * imag = spec.getImag();
                
                for( int j=0; j<spec.getNumFrames(); j++ ){
                    vec3 p = pos[j];
                    
                    float prm = p.z;
                    real[j] = p.x * prm * 400.0;
                    imag[j] = p.y * prm * 400.0;
                }
                
                //real[0] = 0;
                //imag[0] = 0;
                
                Dft dft;
                audio::Buffer result_wave(dftSize);
                dft.inverse(&spec, &result_wave, dftSize);
                
                float * tmp = result_wave.getChannel(0);
                wavData[i].insert(wavData[i].end(), tmp, tmp+result_wave.getNumFrames()/2-1 );
            }
            
        }
    }
}

void cApp::write_audio(){
    
    for( int i=0; i<wavData.size(); i++ ){
        const vector<float> & w = wavData[i];

        if( w.size() > 0){

            string zs = bZsort ? "_zsort" : "";
            string log = rms[i].eStretch==0 ? "_linear" : "_log";
            
            fs::path path = "7.1_idft_" + Ramses::prm[i] + log + zs + ".wav";
            cout << "start writting Wav file : " << path.filename() << endl;
            
            SoundWriter sw;
            sw.writeWav24f( w, 1, outSamplingRate, w.size(), path.string() );
        }
    }
}

void cApp::draw(){
    
    gl::enableAlphaBlending();
    glPointSize(1);
    glLineWidth(1);
    
    bOrtho ? mExp.beginOrtho( true ) : mExp.begin( camUi.getCamera() ); {
        
        gl::clear();    
        gl::enableDepthRead();
        gl::enableDepthWrite();
        
        if( !mExp.bRender && !mExp.bSnap ){
            mt::drawCoordinate(10);
            //gl::drawLine( vec3(0,0,-500), vec3(0,0,500));
            //gl::color(1, 0, 0);
            //gl::drawStrokedCircle( Vec2i(0,0), 20);
        }
        
        for( int i=0; i<rms.size(); i++){
            rms[rms.size()-i-1].draw();
        }
    }mExp.end();
    
    mExp.draw();
    
    if(gui) gui->draw();

    if( bStart)frame++;
}

void cApp::keyDown( KeyEvent event ) {
    char key = event.getChar();
    switch (key) {
        case 'S': mExp.startRender();  break;
        case 'T': mExp.stopRender();  break;
        case 's': mExp.snapShot();  break;
        case ' ': bStart = !bStart; break;
    }
}

void cApp::mouseDown( MouseEvent event ){
    camUi.mouseDown( event.getPos() );
}

void cApp::mouseDrag( MouseEvent event ){
    camUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}

void cApp::resize(){
    CameraPersp & cam = const_cast<CameraPersp&>(camUi.getCamera());
    cam.setAspectRatio( getWindowAspectRatio() );
    camUi.setCamera( &cam );
}

CINDER_APP( cApp, RendererGl( RendererGl::Options().msaa( 0 )) )
