#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include "Config.hpp"

using namespace geode::prelude;

class SettingsPopup : public geode::Popup {
protected:
    TextInput* m_url = nullptr;
    TextInput* m_key = nullptr;
    TextInput* m_model = nullptr;
    TextInput* m_extra = nullptr;

    struct Preset { char const* name; char const* url; char const* model; };
    static constexpr Preset PRESETS[4] = {
        {"OpenAI", "https://api.openai.com/v1", "gpt-4o"},
        {"Groq", "https://api.groq.com/openai/v1", "llama-3.3-70b-versatile"},
        {"OpenRouter", "https://openrouter.ai/api/v1", "openai/gpt-4o"},
        {"Gemini", "https://generativelanguage.googleapis.com/v1beta/openai", "gemini-2.0-flash"},
    };

    CCLabelBMFont* addLabel(char const* text, float y) {
        auto l = CCLabelBMFont::create(text, "goldFont.fnt");
        l->setScale(.45f);
        l->setAnchorPoint({0.f, .5f});
        m_mainLayer->addChildAtPosition(l, Anchor::Center, {-170.f, y});
        return l;
    }

    TextInput* addInput(char const* hint, float y, std::string const& value) {
        auto in = TextInput::create(340.f, hint, "bigFont.fnt");
        in->setMaxCharCount(2000);
        in->setString(value);
        m_mainLayer->addChildAtPosition(in, Anchor::Center, {0.f, y});
        return in;
    }

    bool init() {
        if (!Popup::init(400.f, 290.f)) return false;
        this->setTitle("AI Settings");

        for (int i = 0; i < 4; i++) {
            auto spr = ButtonSprite::create(PRESETS[i].name, "bigFont.fnt", "GJ_button_04.png", .6f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SettingsPopup::onPreset));
            btn->setTag(i);
            m_buttonMenu->addChildAtPosition(btn, Anchor::Center, {-135.f + 90.f * i, 92.f});
        }

        addLabel("API URL (base, e.g. .../v1 or .../v3)", 65.f);
        m_url = addInput("https://api.openai.com/v1", 47.f, cfg::get("url", "https://api.openai.com/v1"));

        addLabel("API Key", 22.f);
        m_key = addInput("sk-... / gsk_...", 4.f, cfg::get("api-key"));
        m_key->setPasswordMode(true);

        addLabel("Model", -21.f);
        m_model = addInput("gpt-4o", -39.f, cfg::get("model", "gpt-4o"));

        addLabel("Extra deco object IDs (optional, e.g. 211,212,1583)", -64.f);
        m_extra = addInput("comma separated ids", -82.f, cfg::get("extra-ids"));

        auto set = ButtonSprite::create("Set", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto setBtn = CCMenuItemSpriteExtra::create(set, this, menu_selector(SettingsPopup::onSet));
        m_buttonMenu->addChildAtPosition(setBtn, Anchor::Bottom, {0.f, 22.f});
        return true;
    }

    void onPreset(CCObject* s) {
        int i = static_cast<CCNode*>(s)->getTag();
        if (i < 0 || i > 3) return;
        m_url->setString(PRESETS[i].url);
        m_model->setString(PRESETS[i].model);
    }

    void onSet(CCObject*) {
        auto url = cfg::trim(m_url->getString());
        auto key = cfg::trim(m_key->getString());
        auto model = cfg::trim(m_model->getString());
        if (url.empty() || key.empty() || model.empty()) {
            Notification::create("Fill URL, API key and model!", NotificationIcon::Error)->show();
            return;
        }
        cfg::set("url", url);
        cfg::set("api-key", key);
        cfg::set("model", model);
        cfg::set("extra-ids", cfg::trim(m_extra->getString()));
        Notification::create("Settings saved!", NotificationIcon::Success, 1.f)->show();
        this->onClose(nullptr);
    }

public:
    static SettingsPopup* create() {
        auto ret = new SettingsPopup();
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
};
