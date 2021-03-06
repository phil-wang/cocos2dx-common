/****************************************************************************
 Author: Luma (stubma@gmail.com)
 
 https://github.com/stubma/cocos2dx-common
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#ifndef __CCResourceLoader_h__
#define __CCResourceLoader_h__

#include "cocos2d.h"
#include "CCResourceLoaderListener.h"
#include "CCLocalization.h"

using namespace std;

NS_CC_BEGIN

/**
 * A self-retain class for resource loading. It schedule resource loading in OpenGL thread in
 * every tick. One resource is handled by one Task, the loading logic is encapsulated in task so
 * you don't care about that. For CPU intensive task, you can set idle time to avoid blocking OpenGL
 * thread too long. If you display an animation feedback, don't use CCAction mechanism because a long 
 * task will cause animation to skip frames. The better choice is invoking setDisplayFrame one by one.
 *
 * \par
 * Decryption is supported and you can provide a decrypt function pointer to load method. Of course you
 * need write an independent tool to encrypt your resources, that's your business.
 *
 * \par
 * Resources supported
 * <ul>
 * <li>Android style strings xml file. It is handy tool to use it with CCLocalization</li>
 * <li>Single image file, encrypted or not</li>
 * <li>Atlas image file, encrypted or not</li>
 * <li>atlas animation</li>
 * <li>Audio file supported by CocosDenshion</li>
 * </ul>
 * If not supported, just adding a task to support it.
 */
class CC_DLL CCResourceLoader : public CCObject {
public:
	/// load parameter
    struct LoadTask {
        /// idle time after loaded
        float idle;
        
        LoadTask() : idle(0.1f) {
        }
        
        virtual ~LoadTask() {}
        
        /// do loading
        virtual void load() {}
    };
    
    /// decrypted function pointer
    typedef const char* (*DECRYPT_FUNC)(const char*, int, int*);
	
private:
    /// type of load operation
    enum ResourceType {
        IMAGE,
        ZWOPTEX,
        ANIMATION
    };
    
    /// android string load task
    struct AndroidStringLoadTask : public LoadTask {
        /// language
        string lan;
        
        /// file path
        string path;
        
        /// merge or not
        bool merge;
        
        virtual ~AndroidStringLoadTask() {}
        
        virtual void load() {
            CCLocalization::sharedLocalization()->addAndroidStrings(lan, path, merge);
        }
    };
	
	/// cocosdenshion music load parameter
    struct CDMusicTask : public LoadTask {
        /// image name
        string name;
        
        virtual ~CDMusicTask() {}
        
        virtual void load();
    };
	
	/// cocosdenshion effect load parameter
    struct CDEffectTask : public LoadTask {
        /// image name
        string name;
        
        virtual ~CDEffectTask() {}
        
        virtual void load();
    };
    
    /// image load parameter
    struct ImageLoadTask : public LoadTask {
        /// image name
        string name;
        
        virtual ~ImageLoadTask() {}
        
        virtual void load() {
            CCTextureCache::sharedTextureCache()->addImage(name.c_str());
        }
    };
	
	/// encrypted image load parameter
    struct EncryptedImageLoadTask : public LoadTask {
        /// image name
        string name;
		
		/// decrypt function
		DECRYPT_FUNC func;
        
        virtual ~EncryptedImageLoadTask() {}
        
        virtual void load() {
			// load encryptd data
            unsigned long len;
            char* data = (char*)CCFileUtils::sharedFileUtils()->getFileData(name.c_str(), "rb", &len);
            
            // create texture
            int decLen;
            const char* dec = NULL;
            if(func) {
                dec = (*func)(data, len, &decLen);
            } else {
                dec = data;
                decLen = (int)len;
            }
            CCImage* image = new CCImage();
            image->initWithImageData((void*)dec, decLen);
            image->autorelease();
            CCTextureCache::sharedTextureCache()->addUIImage(image, name.c_str());
            
            // free
            if(dec != data)
                free((void*)dec);
            free(data);
        }
    };
    
    /// zwoptex load parameter
    struct ZwoptexLoadTask : public LoadTask {
        /// plist name
        string name;
        
        virtual ~ZwoptexLoadTask() {}
        
        virtual void load() {
            CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(name.c_str());
        }
    };
    
    /// encrypted zwoptex load task
    struct EncryptedZwoptexLoadTask : public LoadTask {
        /// plist name, plist is not encrypted
        string name;
        
        /// texture name, which is encrypted
        string texName;
        
        // decrypt func
        DECRYPT_FUNC func;
        
        virtual ~EncryptedZwoptexLoadTask() {}
        
        virtual void load() {
            // load encryptd data
            unsigned long len;
            char* data = (char*)CCFileUtils::sharedFileUtils()->getFileData(texName.c_str(), "rb", &len);
            
            // create texture
            int decLen;
            const char* dec = NULL;
            if(func) {
                dec = (*func)(data, len, &decLen);
            } else {
                dec = data;
                decLen = (int)len;
            }
            CCImage* image = new CCImage();
            image->initWithImageData((void*)dec, decLen);
            image->autorelease();
            CCTexture2D* tex = CCTextureCache::sharedTextureCache()->addUIImage(image, texName.c_str());
            
            // free
            if(dec != data)
                free((void*)dec);
            free(data);
            
            // add zwoptex
            CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(name.c_str(), tex);
        }
    };
    
    /// zwoptex animation load parameter
    struct ZwoptexAnimLoadTask : public LoadTask {
        /// frame list
        typedef vector<string> StringList;
        StringList frames;
        
        /// animation name
        string name;
        
        /// animation unit delay
        float unitDelay;
		
		/// restore original frame when animate is done
		bool restoreOriginalFrame;
		
		ZwoptexAnimLoadTask() :
				unitDelay(0),
				restoreOriginalFrame(false) {
		}
        
        virtual ~ZwoptexAnimLoadTask() {}
        
        virtual void load() {
			if(!CCAnimationCache::sharedAnimationCache()->animationByName(name.c_str())) {
				CCSpriteFrameCache* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
				CCArray* array = CCArray::create();
				for(StringList::iterator iter = frames.begin(); iter != frames.end(); iter++) {
					CCSpriteFrame* f = cache->spriteFrameByName(iter->c_str());
					array->addObject(f);
				}
				CCAnimation* anim = CCAnimation::createWithSpriteFrames(array, unitDelay);
				anim->setRestoreOriginalFrame(restoreOriginalFrame);
				CCAnimationCache::sharedAnimationCache()->addAnimation(anim, name.c_str());
			}
        }
    };
    
    /// zwoptex animation load parameter
    /// it can specify duration for every single frame
    struct ZwoptexAnimLoadTask2 : public LoadTask {
        /// frame list
        typedef vector<string> StringList;
        StringList frames;
        
        /// duration list
        typedef vector<float> TimeList;
        TimeList durations;
        
        /// restore original frame when animate is done
		bool restoreOriginalFrame;
        
        /// animation name
        string name;
        
        ZwoptexAnimLoadTask2() :
                restoreOriginalFrame(false) {
		}
        
        virtual ~ZwoptexAnimLoadTask2() {}
        
        virtual void load() {
            if(!CCAnimationCache::sharedAnimationCache()->animationByName(name.c_str())) {
                CCSpriteFrameCache* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
                CCArray* array = CCArray::create();
                int size = frames.size();
				for(int i = 0; i < size; i++) {
					CCSpriteFrame* sf = cache->spriteFrameByName(frames.at(i).c_str());
                    float& delay = durations.at(i);
                    CCAnimationFrame* af = new CCAnimationFrame();
                    af->initWithSpriteFrame(sf, delay, NULL);
                    af->autorelease();
					array->addObject(af);
				}
                CCAnimation* anim = CCAnimation::createWithSpriteFrames(array, 1);
				anim->setRestoreOriginalFrame(restoreOriginalFrame);
                CCAnimationCache::sharedAnimationCache()->addAnimation(anim, name.c_str());
            }
        }
    };
 
private:
	/// listener
	CCResourceLoaderListener* m_listener;

	/// remaining delay time
    float m_remainingIdle;
    
    /// next loading item
    int m_nextLoad;
    
    /// load list
    typedef vector<LoadTask*> LoadTaskPtrList;
    LoadTaskPtrList m_loadTaskList;

private:
	/// perform loading
	void doLoad(float delta);

public:
    CCResourceLoader(CCResourceLoaderListener* listener);
	virtual ~CCResourceLoader();
	
	/**
	 * a static method used to load an encrypted image
	 *
	 * @param name name of image file, it should be encrypted
	 * @param decFunc decrypt func
	 */
	static void loadImage(const string& name, DECRYPT_FUNC decFunc);
    
	/**
	 * a static method used to load an encrypted zwoptex resource, the plist should not be encrypted
	 *
	 * @param plistName name of plist file, it should not be encrypted
	 * @param texName name of image file, it should be encrypted
	 * @param decFunc decrypt func
	 */
	static void loadZwoptex(const string& plistName, const string& texName, DECRYPT_FUNC decFunc);
	
    /// start loading
    void run();
    
    /// directly add a load task
    void addLoadTask(LoadTask* t);
    
    /**
     * add an Android string loading task
     *
     * @param lan language ISO 639-1 code
     * @param path string XML file platform-independent path
     * @param merge true means merge new strings, or false means replace current strings
     */
    void addAndroidStringTask(const string& lan, const string& path, bool merge = false);
	
	/// add a image loading task
	void addImageTask(const string& name, float idle = 0);
	
	/**
	 * add a image task, but the texture is encrypted. So a decrypt function must be provided.
	 *
	 * @param name name of image file, it should be encrypted
	 * @param decFunc decrypt func
	 * @param idle idle time after loaded
	 */
	void addImageTask(const string& name, DECRYPT_FUNC decFunc, float idle = 0);
	
	/// add a zwoptex image loading task
	void addZwoptexTask(const string& name, float idle = 0);
	
	/**
	 * add a zwoptex task, but the texture is encrypted. So a decrypt function must be provided.
	 *
	 * @param plistName name of plist file, it should not be encrypted
	 * @param texName name of image file, it should be encrypted
	 * @param decFunc decrypt func
	 * @param idle idle time after loaded
	 */
	void addZwoptexTask(const string& plistName, const string& texName, DECRYPT_FUNC decFunc, float idle = 0);
	
	/// add a cocosdenshion effect task
	void addCDEffectTask(const string& name, float idle = 0);
	
	/// add a cocosdenshion music task
	void addCDMusicTask(const string& name, float idle = 0);
	
	/// add a zwoptex animation loading task
	/// the endIndex is inclusive
	void addZwoptexAnimTask(const string& name,
							float unitDelay,
							const string& pattern,
							int startIndex,
							int endIndex,
							bool restoreOriginalFrame = false,
							float idle = 0);
	
	/// add a zwoptex animation loading task
	/// the endIndex is inclusive
	/// this method can specify two sets of start/end index so the
	/// animation can have two stage
	void addZwoptexAnimTask(const string& name,
							float unitDelay,
							const string& pattern,
							int startIndex,
							int endIndex,
							int startIndex2,
							int endIndex2,
							bool restoreOriginalFrame = false,
							float idle = 0);
    
    /**
     * add a zwoptex animation loading task, you can specify delay for every frame
     *
     * @param name animation name
     * @param pattern sprite frame pattern, something likes frame_%d.png, the parameter
     *      must be an integer
     * @param startIndex sprite frame pattern start index
     * @param endIndex sprite frame pattern end index
     * @param delays delay time array, every element is a CGFloat
     * @param restoreOriginalFrame restore original frame or not
     * @param idle idle time after task is completed
     */
    void addZwoptexAnimTask(const string& name,
							const string& pattern,
							int startIndex,
							int endIndex,
                            const CCArray& delays,
							bool restoreOriginalFrame = false,
                            float idle = 0);
	
	/// delay time before start to load
	CC_SYNTHESIZE(float, m_delay, Delay);
};

NS_CC_END

#endif // __CCResourceLoader_h__
