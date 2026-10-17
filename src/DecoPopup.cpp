#include "DecoPopup.hpp"
#include "SettingsPopup.hpp"
#include <Geode/ui/Notification.hpp>
#include <algorithm>
#include <cmath>
#include <map>

static constexpr float GRID = 30.f;

struct Placed {
    int id;
    double x, y, r, sx, sy;
    bool fx, fy;
    int z;
    bool hasZ;
};

static std::string systemPrompt() {
    return R"SYS(You are a senior Geometry Dash (2.2) level decorator. You think like an architect and a level designer.
You get a STRUCTURE (ASCII map) and a STYLE. You do NOT write hundreds of coordinates. You write a short list of high-level BUILD OPS and the game expands them into objects.

FIRST form a concept of what the style really looks like as a silhouette. Example: "temple" = stepped base (stairs), a row of columns, a thick beam on top of the columns, a triangular/stepped roof, side braziers, trims. Then express that concept with ops, hugging the given structure.

COORDINATES
- Grid units. One cell = 1 unit. Cell centers are at integer coordinates. (0,0) is the center of the bottom-left selected object. x goes right, y goes up.
- The MAP shows the selection: letters = occupied cells, '.' = empty. A legend maps letters to ids. The map has 2 cells of margin on every side; the ruler under it shows the last digit of x. Rows are labelled with y.

OPS (JSON objects)
{"op":"rect","id":1,"x":0,"y":0,"w":6,"h":1,"mode":"stretch","z":-4}
  A region whose bottom-left cell center is (x,y), size w by h cells.
  mode "stretch" (default): ONE block stretched to fill the region. Best for clean walls, pillars, beams, roofs, backdrops. Use mostly this.
  mode "fill": tiles of blocks. mode "outline": tiles only along the border. Optional "s" = tile size (default 1).
{"op":"line","id":40,"x":0,"y":0,"dx":1,"dy":1,"n":5,"s":1,"r":0,"z":-3}
  n objects starting at (x,y), stepping (dx,dy): stairs, rows, columns, diagonals, repeated trims.
{"op":"obj","id":1,"x":2,"y":3,"r":45,"sx":1,"sy":1,"fx":false,"fy":false,"z":-2}
  A single object.
{"op":"mirror","axis":3}
  Mirrors EVERYTHING built so far across the vertical line x=axis (flips and rotations are handled). Build one half, then mirror, for perfectly symmetric designs. Use the structure's center as axis.

RULES
1. Use ONLY allowed ids. id 1 is a full square block, id 40 is a half-height slab. Other ids may look different: use them for accents and trims.
2. z = layer order. Negative = behind, positive = in front. Backdrop -8..-5, main deco -4..-1, front accents 1..4.
3. Do not cover the structure itself. Wrap it: trims along its top and sides, base, roof or crown above it, supports beside it, backdrop shapes behind it (negative z).
4. Work big to small: backdrop silhouettes, then main shapes, then trims and small accents. About 15 to 50 ops. Quality over quantity. Keep everything tidy and symmetric, with positions on whole or half units.
5. Follow the STYLE literally. If the style names a thing (temple, castle, forest, tower, cyberpunk city), the result must clearly read as that thing.

OUTPUT: ONE JSON object only, no prose, no markdown, no code fences:
{"concept":"one sentence describing the design","ops":[ ... ]})SYS";
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

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    std::set<int> selIds;
    for (auto& o : m_sel) {
        minX = std::min(minX, o.x); minY = std::min(minY, o.y);
        maxX = std::max(maxX, o.x); maxY = std::max(maxY, o.y);
        m_allowed.insert(o.id);
        selIds.insert(o.id);
    }
    m_origin = CCPoint(minX, minY);
    m_wGrid = (maxX - minX) / GRID;
    m_hGrid = (maxY - minY) / GRID;
    for (int i : {1, 2, 3, 4, 5, 6, 7, 40}) m_allowed.insert(i);
    for (int i : cfg::parseIds(cfg::get("extra-ids"))) m_allowed.insert(i);

    std::string idText;
    for (int i : selIds) {
        if (idText.size() > 38) { idText += "..."; break; }
        idText += (idText.empty() ? "" : ",") + std::to_string(i);
    }
    auto info = CCLabelBMFont::create(
        fmt::format("{} objects | ids: {}", m_sel.size(), idText).c_str(), "chatFont.fnt");
    info->setScale(.75f);
    m_mainLayer->addChildAtPosition(info, Anchor::Top, {0.f, -45.f});

    m_prompt = TextInput::create(330.f, "Describe the deco: temple, neon cyberpunk, spooky forest...", "chatFont.fnt");
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
    else if (m_elapsed < 10.f) msg = "Planning the design";
    else if (m_elapsed < 30.f) msg = "Designing deco layers";
    else msg = fmt::format("Still thinking ({}s)", static_cast<int>(m_elapsed));
    setStatus(msg + d);
}

std::string DecoPopup::buildUserMessage(std::string const& style) {
    std::string s = "STYLE: " + (style.empty() ? std::string("clean modern") : style) + "\n";
    int W = static_cast<int>(std::lround(m_wGrid)) + 1;
    int H = static_cast<int>(std::lround(m_hGrid)) + 1;
    s += fmt::format("Selection size: {} cells wide, {} cells tall. Center axis x = {:.1f}.\n", W, H, (W - 1) / 2.0);
    s += "Allowed ids: ";
    bool first = true;
    for (int i : m_allowed) { s += (first ? "" : ",") + std::to_string(i); first = false; }
    int maxObjs = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
    s += fmt::format("\nMaximum objects: {}\n", maxObjs);

    if (W > 80 || H > 45) {
        s += "MAP omitted (structure too large). Selected objects as [id,x,y]:\n";
        size_t n = m_sel.size();
        size_t stride = n > 200 ? (n + 199) / 200 : 1;
        for (size_t i = 0; i < n; i += stride) {
            auto& o = m_sel[i];
            s += fmt::format("[{},{},{}]", o.id,
                static_cast<int>(std::lround((o.x - m_origin.x) / GRID)),
                static_cast<int>(std::lround((o.y - m_origin.y) / GRID)));
        }
        return s;
    }

    std::vector<std::string> g(H, std::string(W, '.'));
    std::map<int, char> sym;
    char const* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (auto& o : m_sel) {
        int ix = static_cast<int>(std::lround((o.x - m_origin.x) / GRID));
        int iy = static_cast<int>(std::lround((o.y - m_origin.y) / GRID));
        if (ix < 0 || iy < 0 || ix >= W || iy >= H) continue;
        if (!sym.count(o.id)) {
            char ch = sym.size() < 26 ? letters[sym.size()] : '#';
            sym[o.id] = ch;
        }
        g[iy][ix] = sym[o.id];
    }
    s += "Legend: ";
    for (auto& [id, ch] : sym) s += fmt::format("{}=id{} ", ch, id);
    s += "\nMAP (rows are y from top to bottom, 2 cells of margin, first column is x=-2):\n";
    for (int y = H + 1; y >= -2; y--) {
        std::string row(2, '.');
        if (y >= 0 && y < H) row += g[y]; else row += std::string(W, '.');
        row += "..";
        s += fmt::format("{:>4} |{}\n", y, row);
    }
    std::string ruler;
    for (int x = -2; x < W + 2; x++) ruler += static_cast<char>('0' + (((x % 10) + 10) % 10));
    s += fmt::format("  x  |{}\n", ruler);
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
    if (content.empty()) { fail("AI returned an empty answer (try raising Max tokens)"); return; }
    applyDeco(content);
}

void DecoPopup::applyDeco(std::string const& content) {
    auto js = extractJson(content);
    if (js.empty()) { fail("AI answer had no JSON, try again"); return; }
    auto parsed = matjson::parse(js);
    if (parsed.isErr()) { fail("AI JSON was broken, try again"); return; }
    matjson::Value root = parsed.unwrap();
    matjson::Value ops = matjson::Value::array();
    if (root.isArray()) {
        ops = root;
    } else if (root.isObject()) {
        if (root.contains("concept")) log::info("AI concept: {}", root["concept"].asString().unwrapOr(""));
        if (root.contains("ops")) ops = root["ops"];
        else if (root.contains("objects")) ops = root["objects"];
    }
    if (!ops.isArray()) { fail("AI JSON had no ops list"); return; }

    auto lel = LevelEditorLayer::get();
    auto ui = EditorUI::get();
    if (!lel || !ui) { fail("Editor not available"); return; }

    bool noTouch = Mod::get()->getSettingValue<bool>("no-touch");
    size_t cap = static_cast<size_t>(Mod::get()->getSettingValue<int64_t>("max-objects"));

    std::vector<Placed> placed;
    auto add = [&](int id, double x, double y, double r, double sx, double sy, bool fx, bool fy, int z, bool hasZ) {
        if (placed.size() >= cap) return;
        placed.push_back({id, x, y, r, sx, sy, fx, fy, z, hasZ});
    };
    auto num = [](matjson::Value& o, char const* k, double d) {
        return o.contains(k) ? o[k].asDouble().unwrapOr(d) : d;
    };
    auto flag = [](matjson::Value& o, char const* k) {
        return o.contains(k) ? o[k].asBool().unwrapOr(false) : false;
    };
    auto str = [](matjson::Value& o, char const* k, std::string const& d) {
        return o.contains(k) ? o[k].asString().unwrapOr(d) : d;
    };

    for (size_t i = 0; i < ops.size() && placed.size() < cap; i++) {
        auto o = ops[i];
        if (!o.isObject()) continue;
        std::string op = str(o, "op", o.contains("id") ? "obj" : "");
        int id = static_cast<int>(num(o, "id", 0));
        double r = num(o, "r", 0.0);
        bool fx = flag(o, "fx"), fy = flag(o, "fy");
        bool hasZ = o.contains("z");
        int z = hasZ ? static_cast<int>(num(o, "z", 0)) : 0;

        if (op == "mirror") {
            double axis = num(o, "axis", 0.0);
            size_t n = placed.size();
            for (size_t k = 0; k < n && placed.size() < cap; k++) {
                Placed p = placed[k];
                if (std::fabs(p.x - axis) < 1e-3) continue;
                p.x = 2.0 * axis - p.x;
                p.fx = !p.fx;
                p.r = -p.r;
                placed.push_back(p);
            }
            continue;
        }

        if (!m_allowed.count(id)) continue;

        if (op == "obj") {
            add(id, num(o, "x", 0), num(o, "y", 0), r, num(o, "sx", 1), num(o, "sy", 1), fx, fy, z, hasZ);
        } else if (op == "line") {
            double x = num(o, "x", 0), y = num(o, "y", 0);
            double dx = num(o, "dx", 1), dy = num(o, "dy", 0);
            int n = std::clamp(static_cast<int>(num(o, "n", 1)), 1, 200);
            double s = std::clamp(num(o, "s", 1.0), 0.1, 20.0);
            for (int k = 0; k < n; k++) add(id, x + k * dx, y + k * dy, r, s, s, fx, fy, z, hasZ);
        } else if (op == "rect") {
            double x = num(o, "x", 0), y = num(o, "y", 0);
            double w = std::clamp(num(o, "w", 1), 0.25, 80.0);
            double h = std::clamp(num(o, "h", 1), 0.25, 80.0);
            std::string mode = str(o, "mode", "stretch");
            double left = x - 0.5, bottom = y - 0.5;
            if (mode == "stretch") {
                add(id, left + w / 2.0, bottom + h / 2.0, r, w, h, fx, fy, z, hasZ);
            } else {
                double s = std::clamp(num(o, "s", 1.0), 0.25, 4.0);
                int nx = std::max(1, static_cast<int>(std::lround(w / s)));
                int ny = std::max(1, static_cast<int>(std::lround(h / s)));
                for (int jy = 0; jy < ny; jy++) {
                    for (int ix = 0; ix < nx; ix++) {
                        if (mode == "outline" && !(ix == 0 || jy == 0 || ix == nx - 1 || jy == ny - 1)) continue;
                        add(id, left + s * (ix + 0.5), bottom + s * (jy + 0.5), r, s, s, fx, fy, z, hasZ);
                    }
                }
            }
        }
    }

    auto created = CCArray::create();
    for (auto& p : placed) {
        CCPoint pos = CCPoint(m_origin.x + static_cast<float>(p.x) * GRID, m_origin.y + static_cast<float>(p.y) * GRID);
        auto obj = lel->createObject(p.id, pos, false);
        if (!obj) continue;
        float sx = std::clamp(static_cast<float>(std::fabs(p.sx)), 0.05f, 80.f);
        float sy = std::clamp(static_cast<float>(std::fabs(p.sy)), 0.05f, 80.f);
        if (p.r != 0.0) obj->setRotation(static_cast<float>(p.r));
        if (sx != 1.f) obj->updateCustomScaleX(sx);
        if (sy != 1.f) obj->updateCustomScaleY(sy);
        if (p.fx) obj->setFlipX(true);
        if (p.fy) obj->setFlipY(true);
        if (p.hasZ) obj->m_zOrder = p.z;
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
