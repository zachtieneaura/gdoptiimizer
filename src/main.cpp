// Tick Leeway - input buffer / coyote time / air modes (uso local y educativo)
// Objetivo: GD 2.2081 / Geode 5.11.x
// Panel nativo de Cocos (sin ImGui): F6 muestra/oculta, F7 on/off, F8 / F9 = buffer +1 / -1.

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <algorithm>

using namespace geode::prelude;

// ---------- Estado compartido (solo hilo principal) ----------
namespace leeway {
    struct State {
        bool enabled = true;
        int  bufferTicks = 3;
        int  coyoteTicks = 0;
        bool panelVisible = false;
        bool showHud = true;

        // runtime
        bool bufferedPending = false;   // click hecho
        bool bufferedIsP1 = true;
        int  bufferedAge = 0;
        bool bufferedReleased = false;  // el jugador ya solto el boton
        bool skipReplay = false;        // el click fue para un orbe: no repetirlo
        bool releaseNext = false;
        bool releaseNextIsP1 = true;
        int  ticksSinceGround = 999;
        int  lastTick = -1;
        int  lastUsedTicks = 0;
        bool injecting = false;
        bool usedThisAttempt = false;
    };

    inline State& get() { static State s; return s; }

    inline void reset() {
        auto& s = get();
        s.bufferedPending = false; s.bufferedAge = 0;
        s.bufferedReleased = false; s.skipReplay = false; s.releaseNext = false;
        s.ticksSinceGround = 999;  s.lastTick = -1;
        s.lastUsedTicks = 0;       s.usedThisAttempt = false;
    }

    inline void syncSettings() {
        auto& s = get();
        s.enabled     = Mod::get()->getSettingValue<bool>("enabled");
        s.showHud     = Mod::get()->getSettingValue<bool>("show-hud");
        s.bufferTicks = static_cast<int>(Mod::get()->getSettingValue<int64_t>("buffer-ticks"));
        s.coyoteTicks = static_cast<int>(Mod::get()->getSettingValue<int64_t>("coyote-ticks"));
    }

    inline void setEnabled(bool v) {
        get().enabled = v;
        Mod::get()->setSettingValue<bool>("enabled", v);
    }

    inline void setBuffer(int v) {
        get().bufferTicks = std::clamp(v, 0, 8);
        Mod::get()->setSettingValue<int64_t>("buffer-ticks", get().bufferTicks);
    }

    // Identifica si el modo de juego es de aire (Wave, Ship, UFO, Swingcopter)
    inline bool isAirMode(PlayerObject* p) {
        return (p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing);
    }

    // Intenta detectar si el jugador esta tocando un orbe.
    template <class T>
    inline bool touchingRing(T* p) {
        if constexpr (requires { p->m_touchingRings->count(); }) {
            return p->m_touchingRings && p->m_touchingRings->count() > 0;
        } else if constexpr (requires { p->m_touchingRings.size(); }) {
            return p->m_touchingRings.size() > 0;
        } else {
            return false;
        }
    }

    inline int currentTick(GJBaseGameLayer* gl) {
        return static_cast<int>(gl->m_gameState.m_currentProgress);
    }
}

// ---------- Logica de juego ----------
class $modify(LeewayGJBGL, GJBaseGameLayer) {

    void handleButton(bool down, int button, bool isPlayer1) {
        auto& s = leeway::get();

        if (!s.enabled || s.injecting || button != 1) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto* p = isPlayer1 ? m_player1 : m_player2;
        if (!p) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // --- LÓGICA PARA MODOS DE AIRE (Wave, Ship, UFO, Swing) ---
        if (leeway::isAirMode(p)) {
            if (!down) {
                return GJBaseGameLayer::handleButton(down, button, isPlayer1);
            }

            if (s.bufferTicks > 0) {
                s.bufferedPending = true;
                s.bufferedIsP1 = isPlayer1;
                s.bufferedAge = 0;
                s.skipReplay = leeway::touchingRing(p);
            }
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // --- LÓGICA TRADICIONAL PARA MODOS DE SUELO (Cubo, Robot, Araña, Bola) ---
        if (!down) {
            if (s.bufferedPending && s.bufferedIsP1 == isPlayer1) {
                s.bufferedReleased = true;
            }
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        if (p->m_isOnGround) {
            s.bufferedPending = false;
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // Coyote ticks para suelo
        if (s.coyoteTicks > 0 && s.ticksSinceGround <= s.coyoteTicks) {
            bool prev = p->m_isOnGround;
            p->m_isOnGround = true;
            s.injecting = true;
            GJBaseGameLayer::handleButton(true, button, isPlayer1);
            s.injecting = false;
            p->m_isOnGround = prev;
            s.lastUsedTicks = s.ticksSinceGround;
            s.usedThisAttempt = true;
            return;
        }

        if (s.bufferTicks > 0) {
            s.bufferedPending = true;
            s.bufferedIsP1 = isPlayer1;
            s.bufferedAge = 0;
            s.bufferedReleased = false;
            s.skipReplay = leeway::touchingRing(p);
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& s = leeway::get();

        if (s.enabled) {
            int tick = leeway::currentTick(this);
            int delta = (s.lastTick < 0) ? 0 : std::max(0, tick - s.lastTick);
            s.lastTick = tick;

            if (s.releaseNext) {
                s.releaseNext = false;
                s.injecting = true;
                GJBaseGameLayer::handleButton(false, 1, s.releaseNextIsP1);
                s.injecting = false;
            }

            if (m_player1) {
                s.ticksSinceGround = m_player1->m_isOnGround ? 0 : s.ticksSinceGround + delta;
            }

            if (s.bufferedPending) {
                s.bufferedAge += delta;
                auto* p = s.bufferedIsP1 ? m_player1 : m_player2;

                if (p) {
                    if (leeway::isAirMode(p)) {
                        // Gestión de retardo por ticks en modos de aire
                        if (s.bufferedAge >= s.bufferTicks) {
                            s.lastUsedTicks = s.bufferedAge;
                            s.usedThisAttempt = true;
                            s.bufferedPending = false;
                        }
                    } else {
                        // Lógica para suelo (Cubo, Robot, Araña, Bola)
                        if (p->m_isOnGround) {
                            if (!s.skipReplay && s.bufferedReleased && s.bufferedAge <= s.bufferTicks) {
                                s.lastUsedTicks = s.bufferedAge;
                                s.usedThisAttempt = true;

                                s.injecting = true;
                                GJBaseGameLayer::handleButton(true, 1, s.bufferedIsP1);
                                s.injecting = false;

                                s.releaseNext = true;
                                s.releaseNextIsP1 = s.bufferedIsP1;
                            }
                            s.bufferedPending = false;
                        } else if (s.bufferedAge > s.bufferTicks) {
                            s.bufferedPending = false;
                        }
                    }
                }
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

// ---------- HUD + panel nativo ----------
class $modify(LeewayPL, PlayLayer) {
    struct Fields {
        CCLabelBMFont* hud = nullptr;
        CCNode* panel = nullptr;
        CCLabelBMFont* info = nullptr;
    };

    void onLeewayToggle(CCObject*) { leeway::setEnabled(!leeway::get().enabled); }
    void onLeewayMinus(CCObject*)  { leeway::setBuffer(leeway::get().bufferTicks - 1); }
    void onLeewayPlus(CCObject*)   { leeway::setBuffer(leeway::get().bufferTicks + 1); }

    void buildPanel() {
        auto win = CCDirector::get()->getWinSize();

        auto* panel = CCNode::create();
        panel->setPosition({win.width - 105.f, win.height / 2.f});
        panel->setZOrder(100);

        auto* bg = CCScale9Sprite::create("square02b_001.png");
        bg->setContentSize({200.f, 120.f});
        bg->setColor({0, 0, 0});
        bg->setOpacity(150);
        panel->addChild(bg);

        auto* title = CCLabelBMFont::create("Tick Leeway", "bigFont.fnt");
        title->setScale(0.45f);
        title->setPosition({0.f, 45.f});
        panel->addChild(title);

        auto* info = CCLabelBMFont::create("", "bigFont.fnt");
        info->setScale(0.4f);
        info->setPosition({0.f, 20.f});
        panel->addChild(info);
        m_fields->info = info;

        auto* menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        auto* tBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("ON/OFF"), this, menu_selector(LeewayPL::onLeewayToggle));
        tBtn->setPosition({0.f, -5.f});
        menu->addChild(tBtn);

        auto* mBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("-"), this, menu_selector(LeewayPL::onLeewayMinus));
        mBtn->setPosition({-40.f, -38.f});
        menu->addChild(mBtn);

        auto* pBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("+"), this, menu_selector(LeewayPL::onLeewayPlus));
        pBtn->setPosition({40.f, -38.f});
        menu->addChild(pBtn);

        panel->addChild(menu);
        panel->setVisible(false);

        m_uiLayer->addChild(panel);
        m_fields->panel = panel;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto* l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(0.35f);
        l->setAnchorPoint({0.f, 1.f});
        auto win = CCDirector::get()->getWinSize();
        l->setPosition({8.f, win.height - 8.f});
        l->setOpacity(180);
        l->setID("leeway-hud"_spr);
        m_uiLayer->addChild(l);
        m_fields->hud = l;

        buildPanel();
        leeway::reset();
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& s = leeway::get();

        if (m_fields->hud) {
            s.showHud = Mod::get()->getSettingValue<bool>("show-hud");
            m_fields->hud->setVisible(s.enabled && s.showHud);
            m_fields->hud->setString(
                fmt::format("Buffer: {}/{} | Usado: {}t",
                            s.bufferedPending ? s.bufferedAge : 0,
                            s.bufferTicks,
                            s.lastUsedTicks).c_str());
        }

        if (m_fields->panel) {
            m_fields->panel->setVisible(s.panelVisible);
            if (s.panelVisible && m_fields->info) {
                m_fields->info->setString(
                    fmt::format("{} | Buffer: {}t", s.enabled ? "ON" : "OFF", s.bufferTicks).c_str());
            }
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        leeway::reset();
    }
};

// ---------- Teclas: F6 panel, F7 on/off, F8 +1, F9 -1 ----------
class $modify(LeewayKB, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
            auto& s = leeway::get();
            switch (key) {
                case KEY_F6: s.panelVisible = !s.panelVisible; return true;
                case KEY_F7: leeway::setEnabled(!s.enabled); return true;
                case KEY_F8: leeway::setBuffer(s.bufferTicks + 1); return true;
                case KEY_F9: leeway::setBuffer(s.bufferTicks - 1); return true;
                default: break;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

$on_mod(Loaded) {
    leeway::syncSettings();
}
