#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <set>
#include <vector>
#include "Config.hpp"

using namespace geode::prelude;

struct SelObj {
    int id;
    float x, y, r, sx, sy;
    bool fx, fy;
    int z;
};

class DecoPopup : public geode::Popup {
protected:
    std::vector<SelObj> m_sel;
    std::set<int> m_allowed;
    CCPoint m_origin = {0.f, 0.f};
    float m_wGrid = 0.f, m_hGrid = 0.f;

    TextInput* m_prompt = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_styleLabel = nullptr;
    CCSprite* m_spinner = nullptr;
    CCMenuItemSpriteExtra* m_goBtn = nullptr;
    async::TaskHolder<web::WebResponse> m_listener;
    bool m_busy = false;
    float m_elapsed = 0.f;

    bool init(std::vector<SelObj> sel);
    void onGo(CCObject*);
    void onSettings(CCObject*);
    void onLearn(CCObject*);
    void onClearStyle(CCObject*);
    void refreshStyleLabel();
    void tick(float dt);
    void setBusy(bool busy);
    void setStatus(std::string const& text, bool error = false);
    void onWeb(web::WebResponse res);
    void fail(std::string const& msg);
    void applyDeco(std::string const& content);
    std::string buildUserMessage(std::string const& style);

public:
    static DecoPopup* create(std::vector<SelObj> sel);
};
