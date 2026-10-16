#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Notification.hpp>
#include "DecoPopup.hpp"

using namespace geode::prelude;

class $modify(AIDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("ai-deco-menu"_spr);
        menu->setPosition({0.f, 0.f});

        auto label = CCLabelBMFont::create("AI", "bigFont.fnt");
        auto spr = CircleButtonSprite::create(label, CircleBaseColor::Pink, CircleBaseSize::Small);
        spr->setScale(.85f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AIDecoEditorUI::onAIDeco));
        btn->setID("ai-deco-button"_spr);
        // spot circled in the screenshot: empty slot left of the Copy button
        btn->setPosition({win.width * 0.806f, win.height * 0.806f});
        menu->addChild(btn);
        this->addChild(menu, 100);
        return true;
    }

    void onAIDeco(CCObject*) {
        std::vector<SelObj> v;
        if (auto arr = this->getSelectedObjects()) {
            for (auto obj : CCArrayExt<GameObject*>(arr)) {
                if (!obj) continue;
                v.push_back({
                    obj->m_objectID,
                    obj->getPositionX(), obj->getPositionY(),
                    obj->getRotation(),
                    std::fabs(obj->getScaleX()), std::fabs(obj->getScaleY()),
                    obj->m_isFlipX, obj->m_isFlipY,
                    obj->m_zOrder
                });
            }
        }
        if (v.empty()) {
            Notification::create("Select some blocks first!", NotificationIcon::Warning)->show();
            return;
        }
        if (auto p = DecoPopup::create(std::move(v))) p->show();
    }
};
