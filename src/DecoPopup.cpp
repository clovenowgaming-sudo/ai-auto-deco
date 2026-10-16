#include "DecoPopup.hpp"
#include "SettingsPopup.hpp"
#include <Geode/ui/Notification.hpp>
#include <cmath>

static constexpr float GRID = 30.f;

static std::string systemPrompt() {
    return R"SYS(You are an expert Geometry Dash (2.2) level decorator with the taste of a top-tier creator.
You get a STRUCTURE (list of objects) and a STYLE request. You output ADDITIONAL decoration objects that make the structure look professionally decorated, like in polished featured levels.

COORDINATES
- Grid units: 1 unit = 1 block = 30 game units. x goes right, y goes up.
- Origin (0,0) is the bottom-left corner of the selection's bounding box. Selection bounds are given.
- Input rows: [id, x, y, rotation, scaleX, scaleY, flipX, flipY, zOrder].

DESIGN RULES
1. Use ONLY object ids from the allowed list. Never invent ids.
2. Build in LAYERS: a back layer (large, low-contrast shapes, negative z), a mid layer (edge pieces, outlines, trims hugging the structure), and a front layer (small accents/highlights, higher z). Lower z = further behind.
3. Hug the structure: decorate along edges, corners and tops, mirror/repeat pieces with a regular rhythm (spacing multiples of 0.5), use symmetry where the structure is symmetric.
4. Vary scale (0.4 - 2.5) to create depth and hierarchy; avoid random noise and avoid piling objects at the exact same position.
5. Snap positions to 0.25 grid. Use rotations that are multiples of 90 unless an organic look is requested; use flips to mirror pieces.
6. Keep the playable shape readable: do not bury the structure under clutter, leave breathing room.
7. Follow the user's STYLE strictly (theme, mood, density). If the style is empty, go for a clean modern look.
8. Quality over quantity, but use a rich amount of objects (at least 40 when the structure is big enough) and never exceed the given maximum.

OUTPUT
Reply with ONE JSON object only. No prose, no markdown, no code fences:
{"objects":[{"id":1,"x":0.5,"y":2,"r":0,"sx":1,"sy":1,"fx":false,"fy":false,"z":-5}]}
Fields: id (int), x, y (grid units), r (degrees), sx, sy (scale), fx, fy (flip), z (z order, optional).)SYS";
}

static std::string extractJson(std::string s) {
    auto t = s.rfind("</think>");
    if (t != std::string::npos) s = s.substr(t + 8);
    auto a = s.find_first_of("{[");
    auto b = s.find_last_of("}]");
    if (a == std::string::npos || b == std::string::npos || b < a) return "";
    return s.substr(a, b - a + 1);
}

DecoPopup* DecoPopup::create(std::vector<SelObj> sel) {
    auto ret = new DecoPopup();
    if (ret->init(std::move(sel))) { ret->autorelease(); return ret; }
    delete ret;
    return nullptr;
}

bool DecoPopup::init(std::vector<SelObj> sel) {
    if (!Popup::init(380.f, 230.f)) return false;
    this->setTitle("AI Auto Deco");
    m_sel = std::move(sel);

    // bounds + allowed ids
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& o : m_sel) {
        minX = std::min(minX, o.x); minY = std::min(minY, o.y);
        maxX = std::max(maxX, o.x); maxY = std::max(maxY, o.y);
        m_allowed.insert(o.id);
    }
    m_origin = {minX, minY};
    m_wGrid = (maxX - minX) / GRID;
    m_hGrid = (maxY - minY) / GRID;
    for (int i : {1, 2, 3, 4, 5, 6, 7, 40}) m_allowed.insert(i);
    for (int i : cfg::parseIds(cfg::get("extra-ids"))) m_allowed.insert(i);

    auto info = CCLabelBMFont::create(fmt::format("{} objects selected", m_sel.size()).c_str(), "chatFont.fnt");
    info->setScale(.8f);
    m_mainLayer->addChildAtPosition(info, Anchor::Top, {0.f, -45.f});

    m_prompt = TextInput::create(330.f, "Describe the deco: neon cyberpunk, spooky forest...", "bigFont.fnt");
    m_prompt->setMaxCharCount(300);
    m_prompt->setString(cfg::get("last-prompt"));
    m_mainLayer->addChildAtPosition(m_prompt, Anchor::Center, {0.f, 22.f});

    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setScale(.4f);
    m_status->setWidth(330.f / .4f);
    m_status->setAlignment(kCCTextAlignmentCenter);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Center, {0.f, -28.f});

    m_spinner = CCSprite::create("loadingCircle.png");
    if (m_spinner) {
        m_spinner->setScale(.35f);
        m_spinner->setVisible(false);
        m_spinner->runAction(CCRepeatForever::create(CCRotateBy::create(1.f, 360.f)));
        m_mainLayer->addChildAtPosition(m_spinner, Anchor::Center, {0.f, -62.f});
    }

    auto go = ButtonSprite::create("Deco!", "goldFont.fnt", "GJ_button_01.png", .9f);
    m_goBtn = CCMenuItemSpriteExtra::create(go, this, menu_selector(DecoPopup::onGo));
    m_buttonMenu->addChildAtPosition(m_goBtn, Anchor::Bottom, {0.f, 26.f});

    auto gear = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
    gear->setScale(.65f);
    auto gearBtn = CCMenuItemSpriteExtra::create(gear, this, menu_selector(DecoPopup::onSettings));
    m_buttonMenu->addChildAtPosition(gearBtn, Anchor::TopRight, {-22.f, -22.f});

    this->schedule(schedule_selector(DecoPopup::tick), .25f);
    return true;
}

void DecoPopup::onSettings(CCObject*) {
    if (m_busy) return;
    SettingsPopup::create()->show();
}

void DecoPopup::setStatus(std::string const& text, bool error) {
    m_status->setString(text.c_str());
    m_status->setColor(error ? ccColor3B{255, 90, 90} : ccColor3B{255, 255, 255});
}

void DecoPopup::setBusy(bool busy) {
    m_busy = busy;
    m_elapsed = 0.f;
    if (m_spinner) m_spinner->setVisible(busy);
    m_goBtn->setEnabled(!busy);
    m_goBtn->setOpacity(busy ? 120 : 255);
    if (m_goBtn->getNormalImage()) {
        if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(m_goBtn->getNormalImage())) rgba->setOpacity(busy ? 120 : 255);
    }
}

void DecoPopup::tick(float dt) {
    if (!m_busy) return;
    m_elapsed += dt;
    int dots = static_cast<int>(m_elapsed * 2.f) % 4;
    std::string d(dots, '.');
    std::string msg;
    if (m_elapsed < 3.f) msg = "Reading structure";
    else if (m_elapsed < 10.f) msg = "Thinking about the design";
    else if (m_elapsed < 30.f) msg = "Designing deco layers";
    else msg = fmt::format("Still thinking ({}s)", static_cast<int>(m_elapsed));
    setStatus(msg + d);
}

std::string DecoPopup::buildUserMessage(std::string const& style) {
    std::string s = "STYLE: " + (style.empty() ? std::string("clean modern") : style) + "\n";
    s += fmt::format("Selection bounds (grid units): width {:.1f}, height {:.1f}\n", m_wGrid, m_hGrid);
    s += "Allowed ids: ";
    bool first = true;
    for (int i : m_allowed) { s += (first ? "" : ",") + std::to_string(i); first = false; }
    int maxObjs = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
    s += fmt::format("\nMaximum deco objects: {}\n", maxObjs);
    s += "STRUCTURE rows [id,x,y,rot,sx,sy,fx,fy,z]:\n[";
    size_t n = m_sel.size();
    size_t stride = n > 250 ? (n + 249) / 250 : 1;
    bool firstRow = true;
    for (size_t i = 0; i < n; i += stride) {
        auto& o = m_sel[i];
        if (!firstRow) s += ",";
        firstRow = false;
        s += fmt::format("[{},{:.2f},{:.2f},{:.0f},{:.2f},{:.2f},{},{},{}]",
            o.id, (o.x - m_origin.x) / GRID, (o.y - m_origin.y) / GRID, o.r, o.sx, o.sy,
            o.fx ? 1 : 0, o.fy ? 1 : 0, o.z);
    }
    s += "]";
    return s;
}

void DecoPopup::onGo(CCObject*) {
    if (m_busy) return;
    if (!cfg::ready()) {
        Notification::create("Set your API key and model first", NotificationIcon::Warning)->show();
        SettingsPopup::create()->show();
        return;
    }
    auto style = cfg::trim(m_prompt->getString());
    cfg::set("last-prompt", style);

    matjson::Value body = matjson::Value::object();
    body["model"] = cfg::trim(cfg::get("model"));
    auto msgs = matjson::Value::array();
    auto sys = matjson::Value::object();
    sys["role"] = "system";
    sys["content"] = systemPrompt();
    auto usr = matjson::Value::object();
    usr["role"] = "user";
    usr["content"] = buildUserMessage(style);
    msgs.push(sys);
    msgs.push(usr);
    body["messages"] = msgs;
    int mt = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-tokens"));
    if (mt > 0) body["max_tokens"] = mt;

    web::WebRequest req;
    req.header("Authorization", "Bearer " + cfg::trim(cfg::get("api-key")));
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(240));

    setBusy(true);
    setStatus("Reading structure");
    m_listener.spawn(req.post(cfg::endpoint()), [this](web::WebResponse res) {
        this->onWeb(std::move(res));
    });
}

void DecoPopup::fail(std::string const& msg) {
    setBusy(false);
    setStatus(msg, true);
    log::error("AI Auto Deco: {}", msg);
}

void DecoPopup::onWeb(web::WebResponse res) {

    if (!res.ok()) {
        std::string detail = res.string().unwrapOr("");
        auto jr = matjson::parse(detail);
        if (jr.isOk()) {
            auto j = jr.unwrap();
            if (j.isObject() && j.contains("error")) {
                auto m = j["error"]["message"].asString();
                if (m.isOk()) detail = m.unwrap();
            }
        }
        if (detail.size() > 140) detail = detail.substr(0, 140) + "...";
        fail(fmt::format("HTTP {}: {}", res.code(), detail));
        return;
    }

    auto jr = res.json();
    if (jr.isErr()) { fail("Response is not valid JSON"); return; }
    auto j = jr.unwrap();
    std::string content;
    if (j.isObject() && j.contains("choices") && j["choices"].isArray() && j["choices"].size() > 0) {
        content = j["choices"][size_t(0)]["message"]["content"].asString().unwrapOr("");
    }
    if (content.empty()) { fail("AI returned an empty answer"); return; }
    applyDeco(content);
}

void DecoPopup::applyDeco(std::string const& content) {
    auto js = extractJson(content);
    if (js.empty()) { fail("AI answer had no JSON, try again"); return; }
    auto parsed = matjson::parse(js);
    if (parsed.isErr()) { fail("AI JSON was broken, try again"); return; }
    matjson::Value root = parsed.unwrap();
    matjson::Value list = root;
    if (root.isObject() && root.contains("objects")) list = root["objects"];
    if (!list.isArray()) { fail("AI JSON had no objects list"); return; }

    auto lel = LevelEditorLayer::get();
    auto ui = EditorUI::get();
    if (!lel || !ui) { fail("Editor not available"); return; }

    bool noTouch = Mod::get()->getSettingValue<bool>("no-touch");
    size_t maxObjs = static_cast<size_t>(Mod::get()->getSettingValue<int64_t>("max-objects"));
    auto created = CCArray::create();

    for (size_t i = 0; i < list.size() && created->count() < maxObjs; i++) {
        auto o = list[i];
        if (!o.isObject()) continue;
        int id = static_cast<int>(o["id"].asInt().unwrapOr(0));
        if (!m_allowed.count(id)) continue;

        double gx = o["x"].asDouble().unwrapOr(0.0);
        double gy = o["y"].asDouble().unwrapOr(0.0);
        float r = static_cast<float>(o["r"].asDouble().unwrapOr(0.0));
        float sx = static_cast<float>(o["sx"].asDouble().unwrapOr(1.0));
        float sy = static_cast<float>(o["sy"].asDouble().unwrapOr(1.0));
        bool fx = o["fx"].asBool().unwrapOr(false);
        bool fy = o["fy"].asBool().unwrapOr(false);
        sx = std::clamp(std::fabs(sx), 0.1f, 10.f);
        sy = std::clamp(std::fabs(sy), 0.1f, 10.f);

        CCPoint pos = {m_origin.x + static_cast<float>(gx) * GRID, m_origin.y + static_cast<float>(gy) * GRID};
        auto obj = lel->createObject(id, pos, false);
        if (!obj) continue;

        if (r != 0.f) obj->setRotation(r);
        if (sx != 1.f) obj->updateCustomScaleX(sx);
        if (sy != 1.f) obj->updateCustomScaleY(sy);
        if (fx) obj->setFlipX(true);
        if (fy) obj->setFlipY(true);
        if (o.contains("z")) obj->m_zOrder = static_cast<int>(o["z"].asInt().unwrapOr(0));
        if (noTouch) obj->m_isNoTouch = true;
        created->addObject(obj);
    }

    setBusy(false);
    if (created->count() == 0) { fail("AI gave no usable objects, try again"); return; }

    ui->deselectAll();
    ui->selectObjects(created, true);
    Notification::create(fmt::format("Added {} deco objects!", created->count()), NotificationIcon::Success)->show();
    this->onClose(nullptr);
}
