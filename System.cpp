#define _CRT_SECURE_NO_WARNINGS
using namespace std;

#include <iostream> 
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <ctime>

//―――― Constants & Helpers ―――――――――――――――――――――――――――――――――――――
const int WINDOW_W = 1280;
const int WINDOW_H = 720;
const float FORM_H = 300.f;
const int MAX_VACCINES = 5;
const int MAX_PETS = 100;
const int MAX_ROUTINES = 100;

float lerp(float a, float b, float t) { return a + (b - a) * t; }
float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
float pulse(float t) { return (sin(t * 2 * 3.14159f) * .5f + .5f) * .3f + .8f; }

int toInt(const char* s) {
    int sign = 1, i = 0, v = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    for (; s[i]; ++i) {
        if (s[i] >= '0' && s[i] <= '9') v = v * 10 + (s[i] - '0');
        else break;
    }
    return v * sign;
}

float scrollX = 0, scrollY = 0;

//―――― Pet Model & Manager ―――――――――――――――――――――――――――――――――――――
struct Pet {
    char name[64], breed[64], medical[128], photo[128];
    int age, vacCount;
    char vaccinations[MAX_VACCINES][32];
    float fade;
};

void parsePet(char* line, Pet& p) {
    char* tok = strtok(line, ",");
    if (tok) strcpy_s(p.name, sizeof(p.name), tok);
    tok = strtok(nullptr, ","); p.age = tok ? toInt(tok) : 0;
    tok = strtok(nullptr, ","); if (tok) strcpy_s(p.breed, sizeof(p.breed), tok);
    tok = strtok(nullptr, ","); if (tok) strcpy_s(p.medical, sizeof(p.medical), tok);
    tok = strtok(nullptr, ","); if (tok) strcpy_s(p.photo, sizeof(p.photo), tok);
    p.vacCount = 0;
    while (p.vacCount < MAX_VACCINES && (tok = strtok(nullptr, ","))) {
        strcpy_s(p.vaccinations[p.vacCount], sizeof(p.vaccinations[0]), tok);
        p.vacCount++;
    }
    p.fade = 1.f;
}

void writePet(std::ofstream& out, const Pet& p) {
    out << p.name << ',' << p.age << ',' << p.breed << ',' << p.medical << ',' << p.photo;
    for (int i = 0; i < p.vacCount; ++i) out << ',' << p.vaccinations[i];
    out << "\n";
}

class PetManager {
    Pet pets[MAX_PETS];
    int petCount;
    const char* fn;
public:
    PetManager(const char* file) :petCount(0), fn(file) {
        load();
        if (petCount == 0) {
            Pet c = {}, d = {};
            strcpy_s(c.name, "Whiskers"); strcpy_s(c.breed, "Siamese");
            c.age = 3; strcpy_s(c.medical, "Healthy"); strcpy_s(c.photo, "cat.png");
            c.vacCount = 1; strcpy_s(c.vaccinations[0], "Rabies"); c.fade = 1.f;
            add(c);
            strcpy_s(d.name, "Buddy"); strcpy_s(d.breed, "Labrador");
            d.age = 5; strcpy_s(d.medical, "Needs diet"); strcpy_s(d.photo, "dog.png");
            d.vacCount = 1; strcpy_s(d.vaccinations[0], "Distemper"); d.fade = 1.f;
            add(d);
        }
    }
    int count()const { return petCount; }
    Pet& at(int i) { return pets[i]; }
    void add(const Pet& p) {
        if (petCount < MAX_PETS) pets[petCount++] = p;
        save();
    }
    void load() {
        std::ifstream in(fn); char line[512];
        petCount = 0;
        while (in.getline(line, 512) && petCount < MAX_PETS) {
            if (!line[0]) continue;
            parsePet(line, pets[petCount++]);
        }
    }
    void save() {
        std::ofstream out(fn);
        for (int i = 0; i < petCount; ++i) writePet(out, pets[i]);
    }
    void remove(int idx) {
        if (idx < 0 || idx >= petCount) return;
        for (int i = idx; i + 1 < petCount; ++i)
            pets[i] = pets[i + 1];
        --petCount;
        save();
    }
};

//―――― View Interface ―――――――――――――――――――――――――――――――――――――
class View {
public:
    virtual ~View() {}
    virtual void handleEvent(const sf::Event&, const sf::RenderWindow&) = 0;
    virtual void update(float) = 0;
    virtual void draw(sf::RenderWindow&) = 0;
};

//―――― TabBar ―――――――――――――――――――――――――――――――――――――
class TabBar {
public:
    enum Tab { Pets, Calendar, Schedule, Stats, Settings } sel;
private:
    sf::Font& font;
    const char* names[5] = { "Pets","Calendar","Schedule","Stats","Settings" };
public:
    TabBar(sf::Font& f) :font(f), sel(Pets) {}
    Tab selTab()const { return sel; }
    void handleClick(const sf::Vector2f& m) {
        if (m.y > 40) return;
        int idx = int(m.x / 200);
        if (idx >= 0 && idx < 5) sel = Tab(idx);
    }
    void update(float) {}
    void draw(sf::RenderWindow& w) {
        for (int i = 0; i < 5; ++i) {
            sf::RectangleShape pill({ 200,40 });
            pill.setPosition(i * 200, 0);
            pill.setFillColor(i == sel ? sf::Color(255, 165, 0) : sf::Color(240, 240, 240));
            w.draw(pill);
            sf::Text t(names[i], font, 18);
            t.setFillColor(sf::Color::Black);
            t.setPosition(i * 200 + 100 - t.getLocalBounds().width / 2, 8);
            w.draw(t);
        }
    }
};

//―――― Toast ―――――――――――――――――――――――――――――――――――――
class Toast {
    sf::Text text; sf::Clock clk; bool active = false;
public:
    void show(const char* msg, sf::Font& f) {
        text.setFont(f); text.setString(msg);
        text.setCharacterSize(16); text.setFillColor(sf::Color::White);
        text.setPosition(WINDOW_W - 20 - text.getLocalBounds().width, WINDOW_H - 50);
        clk.restart(); active = true;
    }
    void draw(sf::RenderWindow& w) {
        if (!active) return;
        float t = clk.getElapsedTime().asSeconds();
        if (t > 2) { active = false; return; }
        float a = clampf(1 - t / 2, 0, 1) * 255;
        sf::RectangleShape bg({ text.getLocalBounds().width + 20,30 });
        bg.setFillColor(sf::Color(0, 0, 0, (sf::Uint8)a));
        bg.setPosition(text.getPosition() - sf::Vector2f(10, 5));
        w.draw(bg);
        auto c = text.getFillColor(); c.a = (sf::Uint8)a; text.setFillColor(c);
        w.draw(text);
    }
};

//―――― Form (Add New Pet) ―――――――――――――――――――――――――――――――――
class Form :public View {
    sf::Font& font; sf::RectangleShape panel;
    sf::Text labels[5]; char inputs[5][128];
    int focus; bool visible; float y;
public:
    Form(sf::Font& f) :font(f), focus(-1), visible(false), y(WINDOW_H) {
        panel.setSize({ WINDOW_W,FORM_H });
        panel.setFillColor({ 50,50,80 });
        panel.setPosition(0, y);
        static const char* L[5] = { "Name:","Age:","Breed:","Medical:","Photo Path:" };
        for (int i = 0; i < 5; ++i) {
            labels[i].setFont(font);
            labels[i].setString(L[i]);
            labels[i].setCharacterSize(18);
            labels[i].setFillColor(sf::Color::White);
            inputs[i][0] = '\0';
        }
    }
    bool isVisible()const { return visible; }
    void toggle() { visible = !visible; }
    void handleEvent(const sf::Event& e, const sf::RenderWindow& win)override {
        if (!visible) return;
        sf::Vector2f m((sf::Vector2i)sf::Mouse::getPosition(win));
        if (e.type == sf::Event::MouseButtonPressed) {
            for (int i = 0; i < 5; ++i)
                if (sf::FloatRect(140, y + 20 + i * 40, 300, 25).contains(m))
                    focus = i;
        }
        if (e.type == sf::Event::TextEntered && focus >= 0) {
            char c = (char)e.text.unicode; int len = strlen(inputs[focus]);
            if (c == 8 && len > 0) inputs[focus][len - 1] = '\0';
            else if (c >= 32 && c < 127 && len < 127) inputs[focus][len] = c, inputs[focus][len + 1] = '\0';
        }
    }
    void update(float)override {
        float tgt = visible ? WINDOW_H - FORM_H : WINDOW_H;
        y = lerp(y, tgt, 0.2f);
        panel.setPosition(0, y);
        for (int i = 0; i < 5; ++i)
            labels[i].setPosition(20, y + 20 + i * 40);
    }
    void draw(sf::RenderWindow& w)override {
        if (!visible) return;
        w.draw(panel);
        for (int i = 0; i < 5; ++i) {
            w.draw(labels[i]);
            sf::RectangleShape box({ 300,25 });
            box.setPosition(140, y + 20 + i * 40);
            box.setFillColor(i == focus ? sf::Color(100, 100, 150) : sf::Color(80, 80, 120));
            w.draw(box);
            sf::Text t(inputs[i], font, 16);
            t.setFillColor(sf::Color::White);
            t.setPosition(145, y + 22 + i * 40);
            w.draw(t);
        }
    }
    void getInputs(Pet& p) {
        strcpy_s(p.name, inputs[0]);
        p.age = toInt(inputs[1]);
        strcpy_s(p.breed, inputs[2]);
        strcpy_s(p.medical, inputs[3]);
        strcpy_s(p.photo, inputs[4]);
        p.vacCount = 0;
    }
};


//―――― PetsView ―――――――――――――――――――――――――――――――――――――
class PetsView : public View {
    PetManager& pm;
    sf::Font& font;
    Toast& toast;
    Form& form;

    // header + title
    sf::RectangleShape headerBar;
    sf::Text           title;

    // Search UI
    sf::RectangleShape searchField;
    sf::Text           searchBufferText;
    std::string        searchBuffer;
    bool               searchActive = false;
    sf::RectangleShape btnSearch, btnClear;
    sf::Text           txtSearch, txtClear;

    // Add Pet button
    sf::RectangleShape btnAdd;
    sf::Text           txtAdd;

    // Trash icon
    sf::Texture        trashTex;
    sf::Sprite         trashSpr;

    // scrolling
    float& scrollX;
    float& scrollY;

    // case‐insensitive substring
    static bool ciFind(const std::string& hay, const std::string& ned) {
        if (ned.empty()) return true;
        auto it = std::search(
            hay.begin(), hay.end(),
            ned.begin(), ned.end(),
            [](char a, char b) { return tolower(a) == tolower(b); }
        );
        return it != hay.end();
    }

public:
    PetsView(PetManager& pm_, sf::Font& f, Toast& t, Form& fm)
        : pm(pm_), font(f), toast(t), form(fm),
        scrollX(::scrollX), scrollY(::scrollY)
    {
        // header
        headerBar.setSize({ WINDOW_W, 60 });
        headerBar.setFillColor({ 0,80,100 });
        headerBar.setPosition(0, 40);

        title.setFont(font);
        title.setString("Pets");
        title.setCharacterSize(28);
        title.setFillColor(sf::Color::White);
        title.setPosition(20, 55);

        // search field
        searchField.setSize({ 300,32 });
        searchField.setPosition(WINDOW_W - 620, 52);
        searchField.setFillColor({ 255,255,255,200 });
        searchField.setOutlineColor({ 200,200,200 });
        searchField.setOutlineThickness(1);

        searchBufferText.setFont(font);
        searchBufferText.setCharacterSize(16);
        searchBufferText.setFillColor({ 30,30,30 });
        searchBufferText.setPosition(searchField.getPosition() + sf::Vector2f(8, 6));

        btnSearch.setSize({ 60,32 });
        btnSearch.setPosition(WINDOW_W - 310, 52);
        btnSearch.setFillColor({ 0,120,215 });
        txtSearch.setFont(font);
        txtSearch.setString("Search");
        txtSearch.setCharacterSize(16);
        txtSearch.setFillColor(sf::Color::White);
        txtSearch.setPosition(btnSearch.getPosition() + sf::Vector2f(8, 6));

        btnClear.setSize({ 60,32 });
        btnClear.setPosition(WINDOW_W - 240, 52);
        btnClear.setFillColor({ 200,50,50 });
        txtClear.setFont(font);
        txtClear.setString("Clear");
        txtClear.setCharacterSize(16);
        txtClear.setFillColor(sf::Color::White);
        txtClear.setPosition(btnClear.getPosition() + sf::Vector2f(12, 6));

        // Add Pet button
        btnAdd.setSize({ 120,32 });
        btnAdd.setPosition(WINDOW_W - 120, 52);
        btnAdd.setFillColor({ 255,165,0 });
        txtAdd.setFont(font);
        txtAdd.setString("Add Pet");
        txtAdd.setCharacterSize(16);
        txtAdd.setFillColor(sf::Color::White);
        txtAdd.setPosition(btnAdd.getPosition() + sf::Vector2f(16, 6));

        // trash icon
        trashTex.loadFromFile("trash.png");
        trashSpr.setTexture(trashTex);
    }

    void handleEvent(const sf::Event& e, const sf::RenderWindow& win) override {
        sf::Vector2f m((sf::Vector2i)sf::Mouse::getPosition(win));

        if (searchActive && e.type == sf::Event::TextEntered) {
            char c = (char)e.text.unicode;
            if (c == 8 && !searchBuffer.empty()) searchBuffer.pop_back();
            else if (c >= 32 && c < 127) searchBuffer.push_back(c);
        }

        if (e.type == sf::Event::MouseButtonPressed) {
            searchActive = searchField.getGlobalBounds().contains(m);

 
            if (btnSearch.getGlobalBounds().contains(m)) {
            }
            if (btnClear.getGlobalBounds().contains(m)) {
                searchBuffer.clear();
                toast.show("Filter cleared", font);
            }
            if (btnAdd.getGlobalBounds().contains(m)) {
                form.toggle();
            }

            int drawn = 0;
            const int cols = 2;
            float cardW = ((float)WINDOW_W - 60.f) / cols;
            for (int i = 0; i < pm.count(); ++i) {
                auto& p = pm.at(i);
                if (!ciFind(p.name, searchBuffer)) continue;
                int r = drawn / cols, c = drawn % cols;
                float x = 20 + scrollX + c * (cardW + 20), y = 120 + scrollY + r * 200;
                sf::FloatRect trashBox(
                    x + cardW - 32, y + 8, 24, 24
                );
                if (trashBox.contains(m)) {
                    if (std::cout << "Delete " << p.name << "? (y/n): ",
                        std::cin.get() == 'y') {
                        pm.remove(i);
                        toast.show("Pet deleted", font);
                    }
                    break;
                }
                ++drawn;
            }
        }
    }

    void update(float dt) override {
        for (int i = 0; i < pm.count(); ++i)
            pm.at(i).fade = clampf(pm.at(i).fade + dt * 2, 0, 1);
    }

    void draw(sf::RenderWindow& w) override {
        // header + buttons
        w.draw(headerBar);
        w.draw(title);
        w.draw(searchField);
        searchBufferText.setString(searchBuffer);
        w.draw(searchBufferText);
        w.draw(btnSearch); w.draw(txtSearch);
        w.draw(btnClear);  w.draw(txtClear);
        w.draw(btnAdd);    w.draw(txtAdd);

        sf::RectangleShape canvas({ (float)WINDOW_W,(float)(WINDOW_H - 100) });
        canvas.setPosition(0, 100);
        canvas.setFillColor({ 240,240,245 });
        w.draw(canvas);

        const int cols = 2;
        float cardW = ((float)WINDOW_W - 60.f) / cols;
        float x0 = 20 + scrollX, y0 = 120 + scrollY;
        int drawn = 0;
        for (int i = 0; i < pm.count(); ++i) {
            auto& p = pm.at(i);
            if (!ciFind(p.name, searchBuffer)) continue;
            int r = drawn / cols, c = drawn % cols;
            float x = x0 + c * (cardW + 20), y = y0 + r * 200;
            float alpha = p.fade * 255;

            sf::RectangleShape card({ cardW,180 });
            card.setPosition(x, y);
            card.setFillColor({ 250,250,250,(sf::Uint8)alpha });
            card.setOutlineColor({ 200,200,200,(sf::Uint8)alpha });
            card.setOutlineThickness(1);
            w.draw(card);

            sf::CircleShape av(40);
            av.setPosition(x + cardW / 2 - 40, y + 10);
            av.setFillColor({ 200,200,200,(sf::Uint8)alpha });
            w.draw(av);

            sf::Text nm(p.name, font, 20);
            nm.setFillColor({ 30,30,30,(sf::Uint8)alpha });
            nm.setPosition(x + 20, y + 100);
            w.draw(nm);

            std::ostringstream ss; ss << p.age << " yrs • " << p.breed;
            sf::Text sub(ss.str(), font, 16);
            sub.setFillColor({ 100,100,100,(sf::Uint8)alpha });
            sub.setPosition(x + 20, y + 130);
            w.draw(sub);

            float bx = x + 20, by = y + 160, maxX = x + cardW - 20;
            for (int j = 0; j < p.vacCount; ++j) {
                sf::Text bt(p.vaccinations[j], font, 14);
                auto tb = bt.getLocalBounds();
                float wBadge = tb.width + 16, hBadge = 24;
                if (bx + wBadge > maxX) {
                    bx = x + 20; by += hBadge + 4; 
                }
                sf::RectangleShape badge({ wBadge,hBadge });
                badge.setPosition(bx, by);
                badge.setFillColor(
                    j == 0 ? sf::Color(180, 240, 200) :
                    j == 1 ? sf::Color(240, 200, 200) :
                    sf::Color(200, 220, 240)
                );
                w.draw(badge);
                bt.setFillColor(sf::Color::White);
                bt.setPosition(bx + 8, by + 4);
                w.draw(bt);
                bx += wBadge + 8;
            }

            trashSpr.setPosition(x + cardW - 32, y + 8);
            trashSpr.setColor(sf::Color(255, 255, 255, (sf::Uint8)alpha));
            trashSpr.setScale(24.f / trashTex.getSize().x,
                24.f / trashTex.getSize().y);
            w.draw(trashSpr);

            ++drawn;
        }
    }
};



//―――― CalendarView ―――――――――――――――――――――――――――――――――――――
class CalendarView : public View {
    sf::Font& font;
    Toast& toast;

    sf::RectangleShape headerBar, gridBg, sidebarBg, agendaBg;
    sf::Text           headerText, instrText, sidebarDate;

    sf::RectangleShape btnPrev, btnNext;
    sf::Text           txtPrev, txtNext;

    sf::RectangleShape btnToggleView;
    sf::Text           txtToggleView;
    bool               agendaView = false;

    int month, year, daysIn, startDow;
    const char* MN[12];
    const char* WD[7];
    sf::Clock   clk;

    struct Task {
        int         day;
        string      title;
        sf::Color   color;
    };
    vector<Task> tasks;

    bool   modalVisible = false;
    int    modalDay;
    string modalBuffer;

    sf::RectangleShape modalBg, modalBox, modalConfirm, modalDelete, modalCancel;
    sf::Text           modalTitle, modalLabel, modalInput, modalConfirmTxt, modalDeleteTxt, modalCancelTxt;

public:
    CalendarView(PetManager&, sf::Font& f, Toast& t)
        : font(f), toast(t)
    {

        headerBar.setSize({ WINDOW_W, 60 });
        headerBar.setFillColor({ 0,80,100 });
        headerBar.setPosition(0, 40);

        headerText.setFont(font);
        headerText.setCharacterSize(28);
        headerText.setFillColor(sf::Color::White);
        headerText.setPosition(20, 55);

        instrText.setFont(font);
        instrText.setString("Click a day to add/edit. Toggle view or navigate months.");
        instrText.setCharacterSize(16);
        instrText.setFillColor({ 200,200,200 });
        instrText.setPosition(20, 105);

        btnPrev.setSize({ 40,40 });
        btnPrev.setFillColor({ 255,165,0 });
        btnPrev.setPosition(WINDOW_W / 2 - 60, 40);
        txtPrev.setFont(font);
        txtPrev.setString("<");
        txtPrev.setCharacterSize(24);
        txtPrev.setFillColor(sf::Color::White);
        txtPrev.setPosition(btnPrev.getPosition() + sf::Vector2f(12, 4));

        btnNext.setSize({ 40,40 });
        btnNext.setFillColor({ 255,165,0 });
        btnNext.setPosition(WINDOW_W / 2 + 20, 40);
        txtNext.setFont(font);
        txtNext.setString(">");
        txtNext.setCharacterSize(24);
        txtNext.setFillColor(sf::Color::White);
        txtNext.setPosition(btnNext.getPosition() + sf::Vector2f(12, 4));

        btnToggleView.setSize({ 120,32 });
        btnToggleView.setFillColor({ 0,120,215 });
        btnToggleView.setPosition(WINDOW_W - 370, 52);
        txtToggleView.setFont(font);
        txtToggleView.setCharacterSize(16);
        txtToggleView.setFillColor(sf::Color::White);
        txtToggleView.setPosition(btnToggleView.getPosition() + sf::Vector2f(8, 6));

        sidebarBg.setSize({ 240, WINDOW_H - 100 });
        sidebarBg.setFillColor({ 250,250,250 });
        sidebarBg.setPosition(WINDOW_W - 240, 100);

        sidebarDate.setFont(font);
        sidebarDate.setCharacterSize(18);
        sidebarDate.setFillColor({ 30,30,30 });
        sidebarDate.setPosition(WINDOW_W - 230, 120);

        gridBg.setFillColor({ 240,240,245 });
        agendaBg.setFillColor(sf::Color::White);
        agendaBg.setOutlineColor({ 200,200,200 });
        agendaBg.setOutlineThickness(1);

        modalBg.setSize({ (float)WINDOW_W,(float)WINDOW_H });
        modalBg.setFillColor({ 0,0,0,150 });
        modalBox.setSize({ 400,220 });
        modalBox.setFillColor(sf::Color::White);
        modalBox.setOutlineColor({ 200,200,200 });
        modalBox.setOutlineThickness(1);
        modalBox.setPosition(WINDOW_W / 2 - 200, WINDOW_H / 2 - 110);

        modalTitle.setFont(font);
        modalTitle.setCharacterSize(20);
        modalTitle.setFillColor(sf::Color::Black);

        modalLabel.setFont(font);
        modalLabel.setString("Title:");
        modalLabel.setCharacterSize(16);
        modalLabel.setFillColor(sf::Color::Black);
        modalLabel.setPosition(modalBox.getPosition() + sf::Vector2f(20, 60));

        modalInput.setFont(font);
        modalInput.setCharacterSize(16);
        modalInput.setFillColor(sf::Color::Black);
        modalInput.setPosition(modalBox.getPosition() + sf::Vector2f(20, 90));

        modalConfirm.setSize({ 100,32 });
        modalConfirm.setFillColor({ 0,200,0 });
        modalConfirm.setPosition(modalBox.getPosition() + sf::Vector2f(40, 150));
        modalConfirmTxt.setFont(font);
        modalConfirmTxt.setString("Save");
        modalConfirmTxt.setCharacterSize(16);
        modalConfirmTxt.setFillColor(sf::Color::White);
        modalConfirmTxt.setPosition(modalConfirm.getPosition() + sf::Vector2f(20, 6));

        modalDelete.setSize({ 100,32 });
        modalDelete.setFillColor({ 200,50,50 });
        modalDelete.setPosition(modalBox.getPosition() + sf::Vector2f(160, 150));
        modalDeleteTxt.setFont(font);
        modalDeleteTxt.setString("Delete");
        modalDeleteTxt.setCharacterSize(16);
        modalDeleteTxt.setFillColor(sf::Color::White);
        modalDeleteTxt.setPosition(modalDelete.getPosition() + sf::Vector2f(12, 6));

        modalCancel.setSize({ 100,32 });
        modalCancel.setFillColor({ 100,100,100 });
        modalCancel.setPosition(modalBox.getPosition() + sf::Vector2f(280, 150));
        modalCancelTxt.setFont(font);
        modalCancelTxt.setString("Cancel");
        modalCancelTxt.setCharacterSize(16);
        modalCancelTxt.setFillColor(sf::Color::White);
        modalCancelTxt.setPosition(modalCancel.getPosition() + sf::Vector2f(15, 6));

        static const char* mnames[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
        static const char* wdays[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
        memcpy(MN, mnames, sizeof mnames);
        memcpy(WD, wdays, sizeof wdays);

        time_t now = time(nullptr);
        tm tmv; localtime_s(&tmv, &now);
        month = tmv.tm_mon; year = tmv.tm_year + 1900;
        recomputeCalendar();
    }

    void recomputeCalendar() {
        static int dim[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        bool leap = (year % 4 == 0 && year % 100) || year % 400;
        daysIn = dim[month] + (month == 1 && leap);
        tm tmv = {}; tmv.tm_mon = month; tmv.tm_year = year - 1900; tmv.tm_mday = 1;
        mktime(&tmv);
        startDow = tmv.tm_wday;
    }

    void handleEvent(const sf::Event& e, const sf::RenderWindow& win) override {
        sf::Vector2f m((sf::Vector2i)sf::Mouse::getPosition(win));

        if (modalVisible) {
            if (e.type == sf::Event::TextEntered) {
                char c = (char)e.text.unicode;
                if (c == 8 && !modalBuffer.empty()) modalBuffer.pop_back();
                else if (c >= 32 && c < 127) modalBuffer.push_back(c);
                modalInput.setString(modalBuffer);
            }
            if (e.type == sf::Event::MouseButtonPressed) {
                if (modalConfirm.getGlobalBounds().contains(m)) {
                    bool found = false;
                    for (auto& t : tasks) if (t.day == modalDay) {
                        t.title = modalBuffer; found = true; break;
                    }
                    if (!found) tasks.push_back({ modalDay,modalBuffer,{0,200,180} });
                    toast.show("Task saved", font);
                    modalVisible = false;
                }
                else if (modalDelete.getGlobalBounds().contains(m)) {
                    tasks.erase(remove_if(tasks.begin(), tasks.end(),
                        [&](auto& t) {return t.day == modalDay; }), tasks.end());
                    toast.show("Task deleted", font);
                    modalVisible = false;
                }
                else if (modalCancel.getGlobalBounds().contains(m)) {
                    modalVisible = false;
                }
            }
            return;
        }

        if (e.type == sf::Event::MouseButtonPressed) {
            if (btnPrev.getGlobalBounds().contains(m)) {
                if (--month < 0) { month = 11; --year; } recomputeCalendar();
            }
            else if (btnNext.getGlobalBounds().contains(m)) {
                if (++month > 11) { month = 0; ++year; } recomputeCalendar();
            }
            else if (btnToggleView.getGlobalBounds().contains(m)) {
                agendaView = !agendaView;
            }
            else {
                float calX = 20, calY = 140, calW = WINDOW_W - 260, calH = WINDOW_H - 160;
                float cw = (calW - 8 * 10) / 7, ch = (calH - 7 * 10) / 6;
                for (int d = 1; d <= daysIn; ++d) {
                    int idx = d + startDow - 1, r = idx / 7, c = idx % 7;
                    float x = calX + 10 + c * (cw + 10), y = calY + 10 + r * (ch + 10);
                    sf::RectangleShape hit({ cw,ch });
                    hit.setPosition(x, y);
                    if (hit.getGlobalBounds().contains(m)) {
                        modalVisible = true;
                        modalDay = d;
                        modalBuffer = "";
                        modalTitle.setString("Day " + to_string(d));
                        modalTitle.setPosition(modalBox.getPosition() + sf::Vector2f(20, 20));
                        modalInput.setString("");
                        break;
                    }
                }
            }
        }
    }

    void update(float) override {}

    void draw(sf::RenderWindow& w) override {
        w.draw(headerBar);
        headerText.setString(string(MN[month]) + " " + to_string(year));
        w.draw(headerText);
        w.draw(instrText);
        w.draw(btnPrev);  w.draw(txtPrev);
        w.draw(btnNext);  w.draw(txtNext);

        time_t now = time(nullptr);
        tm tmv; localtime_s(&tmv, &now);
        sidebarDate.setString("Today: " + to_string(tmv.tm_mday) + " " +
            string(MN[tmv.tm_mon]) + " " + to_string(tmv.tm_year + 1900));
        w.draw(sidebarBg);
        w.draw(sidebarDate);

        // view toggle
        txtToggleView.setString(agendaView ? "Grid View" : "Agenda View");
        w.draw(btnToggleView);
        w.draw(txtToggleView);

        if (!agendaView) {
            float calX = 20, calY = 140, calW = WINDOW_W - 260, calH = WINDOW_H - 160;
            gridBg.setSize({ calW,calH }); gridBg.setPosition(calX, calY);
            w.draw(gridBg);
            for (int i = 0; i < 7; ++i) {
                sf::Text wd(WD[i], font, 16);
                wd.setFillColor({ 80,80,80 });
                wd.setPosition(calX + 10 + i * ((calW - 8 * 10) / 7 + 10), calY - 30);
                w.draw(wd);
            }
            float cw = (calW - 8 * 10) / 7, ch = (calH - 7 * 10) / 6;
            float t = clk.getElapsedTime().asSeconds();
            int today = tmv.tm_mday;
            for (int d = 1; d <= daysIn; ++d) {
                int idx = d + startDow - 1, r = idx / 7, c = idx % 7;
                float x = calX + 10 + c * (cw + 10), y = calY + 10 + r * (ch + 10);
                sf::RectangleShape cell({ cw,ch });
                cell.setPosition(x, y);
                cell.setFillColor(sf::Color::White);
                cell.setOutlineColor({ 220,220,220 });
                cell.setOutlineThickness(1);
                w.draw(cell);
                for (auto& tk : tasks) if (tk.day == d) {
                    sf::CircleShape dot(6);
                    dot.setFillColor(tk.color);
                    dot.setPosition(x + cw - 16, y + 10);
                    w.draw(dot);
                }
                if (d == today) {
                    sf::CircleShape halo(cw * 0.35f * pulse(t));
                    halo.setFillColor({ 0,200,180,50 });
                    halo.setPosition(x + cw * 0.15f, y + ch * 0.15f);
                    w.draw(halo);
                }
                sf::Text dt(to_string(d), font, 16);
                dt.setFillColor({ 60,60,60 });
                dt.setPosition(x + 5, y + 5);
                w.draw(dt);
            }
        }
        else {
            float ax = 20, ay = 140, aw = WINDOW_W - 260, ah = WINDOW_H - 160;
            agendaBg.setSize({ aw,ah }); agendaBg.setPosition(ax, ay);
            w.draw(agendaBg);
            for (size_t i = 0; i < tasks.size(); ++i) {
                sf::RectangleShape strip({ aw - 40,30 });
                strip.setPosition(ax + 20, ay + 10 + i * 40);
                strip.setFillColor(tasks[i].color);
                w.draw(strip);
                sf::Text tt(tasks[i].title + " (Day " + to_string(tasks[i].day) + ")", font, 16);
                tt.setFillColor({ 0,0,0 });
                tt.setPosition(ax + 30, ay + 12 + i * 40);
                w.draw(tt);
            }
        }

        if (modalVisible) {
            w.draw(modalBg);
            w.draw(modalBox);
            w.draw(modalTitle);
            w.draw(modalLabel);
            w.draw(modalInput);
            w.draw(modalConfirm); w.draw(modalConfirmTxt);
            w.draw(modalDelete);  w.draw(modalDeleteTxt);
            w.draw(modalCancel);  w.draw(modalCancelTxt);
        }
    }
};


//―――― ScheduleView ―――――――――――――――――――――――――――――――――――――
class ScheduleView : public View {
    struct Routine {
        int         id;
        std::string name;
        std::string time;
    };

    PetManager& pm;
    sf::Font& font;
    Toast& toast;

    std::vector<Routine>   routines;
    int                     nextId = 1;

    sf::RectangleShape      headerBar;
    sf::Text                headerText;
    sf::RectangleShape      btnAdd;
    sf::Text                txtAdd;

    bool                    modalVisible = false;
    int                     editId = 0;
    char                    bufName[64] = "";
    char                    bufTime[16] = "";
    int                     focusField = 0;

    sf::RectangleShape      modalBg, modalBox, modalConfirm, modalDelete, modalCancel;
    sf::Text                modalTitle,
        lblName, lblTime,
        inpName, inpTime,
        txtConfirm, txtDelete, txtCancel;

public:
    ScheduleView(PetManager& pm_, sf::Font& f, Toast& t)
        : pm(pm_), font(f), toast(t)
    {
        headerBar.setSize({ WINDOW_W, 60 });
        headerBar.setFillColor({ 0,80,100 });
        headerBar.setPosition(0, 40);
        headerText.setFont(font);
        headerText.setString("Schedule");
        headerText.setCharacterSize(28);
        headerText.setFillColor(sf::Color::White);
        headerText.setPosition(20, 55);

        btnAdd.setSize({ 140,32 });
        btnAdd.setFillColor({ 255,165,0 });
        btnAdd.setPosition(WINDOW_W - 160, 50);
        txtAdd.setFont(font);
        txtAdd.setString("Add Routine");
        txtAdd.setCharacterSize(16);
        txtAdd.setFillColor(sf::Color::White);
        txtAdd.setPosition(btnAdd.getPosition() + sf::Vector2f(12, 6));

        modalBg.setSize({ (float)WINDOW_W, (float)WINDOW_H });
        modalBg.setFillColor({ 0,0,0,150 });
        modalBox.setSize({ 400,220 });
        modalBox.setFillColor(sf::Color::White);
        modalBox.setOutlineColor({ 200,200,200 });
        modalBox.setOutlineThickness(1);
        modalBox.setPosition(WINDOW_W / 2 - 200, WINDOW_H / 2 - 110);

        modalTitle.setFont(font);
        modalTitle.setCharacterSize(20);
        modalTitle.setFillColor(sf::Color::Black);
        modalTitle.setPosition(modalBox.getPosition() + sf::Vector2f(20, 20));
        lblName.setFont(font);
        lblName.setString("Name:");
        lblName.setCharacterSize(16);
        lblName.setFillColor(sf::Color::Black);
        lblName.setPosition(modalBox.getPosition() + sf::Vector2f(20, 60));

        lblTime.setFont(font);
        lblTime.setString("Time:");
        lblTime.setCharacterSize(16);
        lblTime.setFillColor(sf::Color::Black);
        lblTime.setPosition(modalBox.getPosition() + sf::Vector2f(20, 100));

        inpName.setFont(font);
        inpName.setCharacterSize(16);
        inpName.setFillColor(sf::Color::Black);
        inpName.setPosition(modalBox.getPosition() + sf::Vector2f(80, 60));

        inpTime.setFont(font);
        inpTime.setCharacterSize(16);
        inpTime.setFillColor(sf::Color::Black);
        inpTime.setPosition(modalBox.getPosition() + sf::Vector2f(80, 100));

        modalConfirm.setSize({ 100,32 });
        modalConfirm.setFillColor({ 0,200,0 });
        modalConfirm.setPosition(modalBox.getPosition() + sf::Vector2f(40, 150));
        txtConfirm.setFont(font);
        txtConfirm.setString("Save");
        txtConfirm.setCharacterSize(16);
        txtConfirm.setFillColor(sf::Color::White);
        txtConfirm.setPosition(modalConfirm.getPosition() + sf::Vector2f(20, 6));

        modalDelete.setSize({ 100,32 });
        modalDelete.setFillColor({ 200,50,50 });
        modalDelete.setPosition(modalBox.getPosition() + sf::Vector2f(160, 150));
        txtDelete.setFont(font);
        txtDelete.setString("Delete");
        txtDelete.setCharacterSize(16);
        txtDelete.setFillColor(sf::Color::White);
        txtDelete.setPosition(modalDelete.getPosition() + sf::Vector2f(12, 6));

        modalCancel.setSize({ 100,32 });
        modalCancel.setFillColor({ 100,100,100 });
        modalCancel.setPosition(modalBox.getPosition() + sf::Vector2f(280, 150));
        txtCancel.setFont(font);
        txtCancel.setString("Cancel");
        txtCancel.setCharacterSize(16);
        txtCancel.setFillColor(sf::Color::White);
        txtCancel.setPosition(modalCancel.getPosition() + sf::Vector2f(15, 6));
    }

    void handleEvent(const sf::Event& e, const sf::RenderWindow& w) override {
        sf::Vector2f m = (sf::Vector2f)sf::Mouse::getPosition(w);

        if (modalVisible) {
            if (e.type == sf::Event::MouseButtonPressed) {
                sf::FloatRect rName(inpName.getPosition(), { 200,20 });
                sf::FloatRect rTime(inpTime.getPosition(), { 100,20 });
                if (rName.contains(m)) focusField = 0;
                else if (rTime.contains(m)) focusField = 1;
            }
            if (e.type == sf::Event::TextEntered) {
                char c = (char)e.text.unicode;
                if (focusField == 0) {
                    if (c == '\b' && strlen(bufName) > 0) bufName[strlen(bufName) - 1] = 0;
                    else if (c >= 32 && c < 127 && strlen(bufName) < 63) strncat(bufName, &c, 1);
                    inpName.setString(bufName);
                }
                else {
                    if (c == '\b' && strlen(bufTime) > 0) bufTime[strlen(bufTime) - 1] = 0;
                    else if (c >= 32 && c < 127 && strlen(bufTime) < 15) strncat(bufTime, &c, 1);
                    inpTime.setString(bufTime);
                }
            }
            if (e.type == sf::Event::MouseButtonPressed) {
                if (modalConfirm.getGlobalBounds().contains(m)) {
                    if (editId) {
                        for (auto& r : routines)
                            if (r.id == editId) {
                                r.name = bufName;
                                r.time = bufTime;
                            }
                    }
                    else {
                        routines.push_back({ nextId++, bufName, bufTime });
                    }
                    toast.show("Routine saved", font);
                    modalVisible = false;
                }
                else if (modalDelete.getGlobalBounds().contains(m) && editId) {
                    routines.erase(std::remove_if(routines.begin(), routines.end(),
                        [&](auto& r) {return r.id == editId; }), routines.end());
                    toast.show("Routine deleted", font);
                    modalVisible = false;
                }
                else if (modalCancel.getGlobalBounds().contains(m)) {
                    modalVisible = false;
                }
            }
            return;
        }


        if (e.type == sf::Event::MouseButtonPressed && btnAdd.getGlobalBounds().contains(m)) {
            editId = 0;
            bufName[0] = bufTime[0] = 0;
            inpName.setString("");
            inpTime.setString("");
            modalTitle.setString("New Routine");
            modalVisible = true;
            focusField = 0;
        }
        if (e.type == sf::Event::MouseButtonPressed) {
            float y0 = 100, h = 60, wW = WINDOW_W - 260;
            for (size_t i = 0; i < routines.size(); ++i) {
                sf::FloatRect box(20, y0 + i * (h + 10), wW, h);
                if (box.contains(m)) {
                    auto& r = routines[i];
                    editId = r.id;
                    strncpy(bufName, r.name.c_str(), 63);
                    strncpy(bufTime, r.time.c_str(), 15);
                    inpName.setString(bufName);
                    inpTime.setString(bufTime);
                    modalTitle.setString("Edit Routine");
                    modalVisible = true;
                    focusField = 0;
                    break;
                }
            }
        }
    }

    void update(float) override {}

    void draw(sf::RenderWindow& w) override {

        w.draw(headerBar);
        w.draw(headerText);
        w.draw(btnAdd); w.draw(txtAdd);


        sf::RectangleShape bg({ (float)WINDOW_W, (float)(WINDOW_H - 100) });
        bg.setPosition(0, 100);
        bg.setFillColor({ 240,240,245 });
        w.draw(bg);

        float y0 = 100, h = 60, wW = WINDOW_W - 260;
        for (size_t i = 0; i < routines.size(); ++i) {
            auto& r = routines[i];
            sf::RectangleShape card({ wW,h });
            card.setPosition(20, y0 + i * (h + 10));
            card.setFillColor(sf::Color::White);
            card.setOutlineColor({ 200,200,200 });
            card.setOutlineThickness(1);
            w.draw(card);

            sf::Text txt(r.name + " @ " + r.time, font, 18);
            txt.setFillColor({ 30,30,30 });
            txt.setPosition(30, y0 + 10 + i * (h + 10));
            w.draw(txt);
        }
        if (modalVisible) {
            w.draw(modalBg);
            w.draw(modalBox);
            w.draw(modalTitle);
            w.draw(lblName); w.draw(inpName);
            w.draw(lblTime); w.draw(inpTime);
            w.draw(modalConfirm); w.draw(txtConfirm);
            w.draw(modalDelete);  w.draw(txtDelete);
            w.draw(modalCancel);  w.draw(txtCancel);
        }
    }
};


//―――― StatsView ―――――――――――――――――――――――――――――――――――――
class StatsView :public View {
    sf::Font& font;
    sf::RectangleShape headerBar;
    sf::Text headerText, btnExport, btnPrint;
    float barData[4] = { 5,7,14,2 }, lineData[7] = { 3.2f,3.4f,3.5f,3.6f,3.7f,3.7f,3.8f };
    float donutData[4] = { 35,25,20,20 }, heatMap[30], ringGoals[4] = { 14,7,21,2 }, ringDone[4] = { 14,6,20,2 };

public:
    StatsView(sf::Font& f) :font(f) {
        headerBar.setSize({ WINDOW_W,60 });
        headerBar.setFillColor(sf::Color(0, 80, 100));
        headerBar.setPosition(0, 40);
        headerText.setFont(f); headerText.setString("Stats");
        headerText.setCharacterSize(28);
        headerText.setFillColor(sf::Color::White);
        headerText.setPosition(20, 55);
        btnExport.setFont(f); btnExport.setString("Export CSV");
        btnExport.setCharacterSize(16); btnExport.setFillColor(sf::Color::White);
        btnExport.setPosition(WINDOW_W - 220, 50);
        btnPrint.setFont(f); btnPrint.setString("Print PDF");
        btnPrint.setCharacterSize(16); btnPrint.setFillColor(sf::Color::White);
        btnPrint.setPosition(WINDOW_W - 110, 50);
        for (int i = 0; i < 30; i++) heatMap[i] = (rand() % 5) / 5.f;
    }
    void handleEvent(const sf::Event& e, const sf::RenderWindow& w)override {
        if (e.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f m((sf::Vector2i)sf::Mouse::getPosition(w));
            if (btnExport.getGlobalBounds().contains(m)) {}
            if (btnPrint.getGlobalBounds().contains(m)) { }
        }
    }
    void update(float)override {}
    void draw(sf::RenderWindow& w)override {
        w.draw(headerBar); w.draw(headerText);

        sf::RectangleShape p1({ 100,30 }), p2({ 100,30 });
        p1.setFillColor(sf::Color(255, 165, 0)); p1.setPosition(WINDOW_W - 230, 50);
        p2.setFillColor(sf::Color(255, 165, 0)); p2.setPosition(WINDOW_W - 120, 50);
        w.draw(p1); w.draw(p2); w.draw(btnExport); w.draw(btnPrint);

        sf::RectangleShape canvas({ (float)WINDOW_W,(float)(WINDOW_H - 100) });
        canvas.setPosition(0, 100); canvas.setFillColor(sf::Color(240, 240, 245));
        w.draw(canvas);

        float m = 20, wC = (WINDOW_W - 3 * m) / 2, hC = (WINDOW_H - 140 - 3 * m) / 2;

  
        {
            sf::RectangleShape c({ wC,hC });
            c.setPosition(m, 100 + m); c.setFillColor(sf::Color::White);
            c.setOutlineColor(sf::Color(200, 200, 200)); c.setOutlineThickness(1);
            w.draw(c);
       
            float bx = m + 20, by = 100 + m + hC - 30;
            for (int i = 0; i < 4; i++) {
                float bh = barData[i] / 14.f * (hC - 60);
                sf::RectangleShape b({ 20,bh });
                b.setPosition(bx, by - bh);
                b.setFillColor(sf::Color(80, 80, 120));
                w.draw(b); bx += 40;
            }
    
            sf::VertexArray line(sf::LineStrip, 7);
            for (int i = 0; i < 7; i++) {
                float lx = m + 20 + i * (wC - 60) / 6;
                float ly = 100 + m + hC - 30 - (lineData[i] - 3.f) / 1.f * (hC - 60);
                line[i].position = { lx,ly };
                line[i].color = sf::Color(0, 200, 180);
            }
            w.draw(line);
        }
      
        {
            float cx = 2 * m + wC + wC / 2, cy = 100 + m + hC / 2, r = 80, start = 0;
            for (int i = 0; i < 4; i++) {
                float sweep = donutData[i] / 100.f * 2 * 3.14159f;
                sf::VertexArray arc(sf::TriangleFan);
                arc.append({ {cx,cy},sf::Color::White });
                for (int a = 0; a <= 30; a++) {
                    float t = start + sweep * (a / 30.f);
                    arc.append({ {cx + cos(t) * r,cy + sin(t) * r},
                               sf::Color(
                                   i == 0 ? 180 : 200,
                                   i == 0 ? 240 : 200,
                                   i == 0 ? 200 : 240
                               ) });
                }
                w.draw(arc);
                start += sweep;
            }
            sf::Text pct("35% Feeding", font, 16);
            pct.setFillColor(sf::Color(30, 30, 30));
            pct.setPosition(cx - pct.getLocalBounds().width / 2, cy - 8);
            w.draw(pct);
        }
    
        {
            sf::RectangleShape c({ wC,hC });
            c.setPosition(m, 120 + 2 * m + hC); c.setFillColor(sf::Color::White);
            c.setOutlineColor(sf::Color(200, 200, 200)); c.setOutlineThickness(1);
            w.draw(c);
            int rows = 5, cols = 6;
            float gx = wC - 40, gy = hC - 40, cw = gx / cols, ch = gy / rows;
            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    int idx = i * cols + j;
                    sf::RectangleShape cell({ cw,ch });
                    float v = idx < 30 ? heatMap[idx] : 0;
                    cell.setFillColor(sf::Color(
                        (sf::Uint8)(200 - v * 100),
                        (sf::Uint8)(200 - v * 100),
                        (sf::Uint8)200
                    ));
                    cell.setPosition(
                        m + 20 + j * cw,
                        120 + 2 * m + hC + 20 + i * ch
                    );
                    w.draw(cell);
                }
            }
        }
    
        {
            float cx = 2 * m + wC + wC / 2, cy = 120 + 2 * m + hC + hC / 2, R = 60;
            for (int k = 0; k < 4; k++) {
                float pct = ringDone[k] / ringGoals[k];
                float sa = -3.14159f / 2;
                sf::VertexArray ring(sf::TriangleStrip);
                for (int a = 0; a <= 30; a++) {
                    float t = sa + pct * 2 * 3.14159f * (a / 30.f);
                    ring.append({ {cx + cos(t) * R,cy + sin(t) * R}, sf::Color(80,80,120) });
                    ring.append({ {cx + cos(t) * (R - 12),cy + sin(t) * (R - 12)}, sf::Color(240,240,245) });
                }
                w.draw(ring);
                std::ostringstream oss;
                oss << (int)ringDone[k] << "/" << (int)ringGoals[k];
                sf::Text lab(oss.str(), font, 14);
                lab.setFillColor(sf::Color(30, 30, 30));
                lab.setPosition(cx - 20, cy - 8);
                w.draw(lab);
                cx += R * 2 + 20;
                if ((k + 1) % 2 == 0) {
                    cx = 2 * m + wC + wC / 2;
                    cy += R * 2 + 20;
                }
            }
        }
    }
};

//―――― SettingsView ―――――――――――――――――――――――――――――――――――――
class SettingsView : public View {
public:
    enum Language { ENGLISH, SPANISH };

private:
    sf::Font& font;
    Toast& toast;         


    sf::RectangleShape headerBar;
    sf::Text           headerText;

    sf::RectangleShape canvas;


    struct Card { sf::RectangleShape bg; sf::Text title; } cards[4];

    bool                darkMode = false;
    sf::RectangleShape  toggleKnob;

    float               fontSize = 18.f;
    sf::RectangleShape  sliderTrack;
    sf::RectangleShape  sliderKnob;

    sf::RectangleShape  backupBtn;
    sf::Text            backupText;

    bool                langModal = false;
    sf::RectangleShape  modalBg, modalBox;
    sf::Text            modalTitle, langOptionEn, langOptionEs, modalCancel;
    Language            currentLang = ENGLISH;

public:
    SettingsView(sf::Font& f, Toast& t)
        : font(f), toast(t)
    {
        headerBar.setSize({ WINDOW_W, 60 });
        headerBar.setFillColor({ 0, 80, 100 });
        headerBar.setPosition(0, 40);

        headerText.setFont(font);
        headerText.setString("Settings");
        headerText.setCharacterSize(28);
        headerText.setFillColor(sf::Color::White);
        headerText.setPosition(20, 55);

        canvas.setSize({ (float)WINDOW_W, (float)(WINDOW_H - 100) });
        canvas.setPosition(0, 100);
        canvas.setFillColor({ 240, 240, 245 });
        const char* labels[4] = { "Dark Mode", "Font Size", "Backup Data", "Language" };
        float gutter = 20.f;
        float cw = (WINDOW_W - 3 * gutter) / 2;
        float ch = (WINDOW_H - 100 - 3 * gutter) / 2;
        for (int i = 0; i < 4; ++i) {
            int row = i / 2, col = i % 2;
            auto& c = cards[i];
            c.bg.setSize({ cw, ch });
            c.bg.setFillColor(sf::Color::White);
            c.bg.setOutlineColor({ 200, 200, 200 });
            c.bg.setOutlineThickness(1);
            c.bg.setPosition(gutter + col * (cw + gutter), 100 + gutter + row * (ch + gutter));

            c.title.setFont(font);
            c.title.setString(labels[i]);
            c.title.setCharacterSize(20);
            c.title.setFillColor({ 30, 30, 30 });
            c.title.setPosition(c.bg.getPosition() + sf::Vector2f(20, 20));
        }

        toggleKnob.setSize({ 24, 24 });
        toggleKnob.setFillColor({ 255, 165, 0 });
        updateToggleKnob();

        sliderTrack.setSize({ cards[1].bg.getSize().x - 40, 4 });
        sliderTrack.setFillColor({ 200, 200, 200 });
        sliderTrack.setPosition(cards[1].bg.getPosition() + sf::Vector2f(20, cards[1].bg.getSize().y / 2));

        sliderKnob.setSize({ 16, 30 });
        sliderKnob.setFillColor({ 255, 165, 0 });
        updateSliderKnob();

        backupBtn = cards[2].bg;
        backupText = cards[2].title;

        modalBg.setSize({ (float)WINDOW_W, (float)WINDOW_H });
        modalBg.setFillColor({ 0, 0, 0, 150 });

        modalBox.setSize({ 400, 200 });
        modalBox.setFillColor(sf::Color::White);
        modalBox.setOutlineColor({ 200, 200, 200 });
        modalBox.setOutlineThickness(1);
        modalBox.setPosition(WINDOW_W / 2 - 200, WINDOW_H / 2 - 100);

        modalTitle.setFont(font);
        modalTitle.setString("Select Language");
        modalTitle.setCharacterSize(20);
        modalTitle.setFillColor(sf::Color::Black);
        modalTitle.setPosition(modalBox.getPosition() + sf::Vector2f(20, 20));

        langOptionEn.setFont(font);
        langOptionEn.setString("English");
        langOptionEn.setCharacterSize(18);
        langOptionEn.setFillColor(sf::Color::Blue);
        langOptionEn.setPosition(modalBox.getPosition() + sf::Vector2f(40, 70));

        langOptionEs.setFont(font);
        langOptionEs.setString("Español");
        langOptionEs.setCharacterSize(18);
        langOptionEs.setFillColor(sf::Color::Black);
        langOptionEs.setPosition(modalBox.getPosition() + sf::Vector2f(40, 110));

        modalCancel.setFont(font);
        modalCancel.setString("Cancel");
        modalCancel.setCharacterSize(16);
        modalCancel.setFillColor(sf::Color::White);
        modalCancel.setPosition(modalBox.getPosition() + sf::Vector2f(300, 160));
    }

    void updateToggleKnob() {
        auto& c = cards[0].bg;
        float x0 = c.getPosition().x + c.getSize().x - 44;
        toggleKnob.setPosition({ x0 + (darkMode ? 20.f : 0.f), c.getPosition().y + 18 });
    }

    void updateSliderKnob() {
        auto& c = cards[1].bg;
        float trackX = c.getPosition().x + 20;
        float trackW = c.getSize().x - 40;
        float x = trackX + (fontSize - 10.f) / 30.f * trackW;
        sliderKnob.setPosition({ x - 8, c.getPosition().y + c.getSize().y / 2 - 15 });
    }

    void handleEvent(const sf::Event& e, const sf::RenderWindow& w) override {
        sf::Vector2f m = (sf::Vector2f)sf::Mouse::getPosition(w);

        if (langModal) {
            if (e.type == sf::Event::MouseButtonPressed) {
                if (langOptionEn.getGlobalBounds().contains(m)) {
                    currentLang = ENGLISH;
                    toast.show("Language: English", font);
                    langModal = false;
                }
                else if (langOptionEs.getGlobalBounds().contains(m)) {
                    currentLang = SPANISH;
                    toast.show("Idioma: Español", font);
                    langModal = false;
                }
                else if (sf::FloatRect(modalBox.getPosition() + sf::Vector2f(300, 160), { 80,30 }).contains(m)) {
                    langModal = false;
                }
            }
            return;
        }
        if (e.type == sf::Event::MouseButtonPressed &&
            cards[0].bg.getGlobalBounds().contains(m)) {
            darkMode = !darkMode;
            updateToggleKnob();
        }
        else if (e.type == sf::Event::MouseButtonPressed &&
            backupBtn.getGlobalBounds().contains(m)) {
            std::ifstream  inF("pets.txt");
            std::ofstream outF("pets_backup.txt");
            outF << inF.rdbuf();
            toast.show("Backup saved", font);
        }
        else if (e.type == sf::Event::MouseButtonPressed &&
            cards[3].bg.getGlobalBounds().contains(m)) {
            langModal = true;
        }
        else if (e.type == sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            if (cards[1].bg.getGlobalBounds().contains(m)) {
                auto& c = cards[1].bg;
                float trackX = c.getPosition().x + 20,
                    trackW = c.getSize().x - 40;
                float x = clampf(m.x, trackX, trackX + trackW);
                fontSize = 10.f + (x - trackX) / trackW * 30.f;
                updateSliderKnob();
            }
        }
    }

    void update(float) override {
        // no continuous animations needed
    }

    void draw(sf::RenderWindow& w) override {
        w.draw(headerBar);
        w.draw(headerText);
        w.draw(canvas);

        for (int i = 0; i < 4; ++i) {
            w.draw(cards[i].bg);
            w.draw(cards[i].title);
        }

        w.draw(toggleKnob);

        w.draw(sliderTrack);
        w.draw(sliderKnob);

        w.draw(backupBtn);
        w.draw(backupText);
        if (langModal) {
            w.draw(modalBg);
            w.draw(modalBox);
            w.draw(modalTitle);
            w.draw(langOptionEn);
            w.draw(langOptionEs);
            sf::RectangleShape cb({ 80,30 });
            cb.setPosition(modalBox.getPosition() + sf::Vector2f(300, 160));
            cb.setFillColor({ 100,100,100 });
            w.draw(cb);
            w.draw(modalCancel);
        }
    }
};


int main() {
    sf::RenderWindow win({ WINDOW_W, WINDOW_H }, "Pet Care Manager");
    win.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font\n";
        return 1;
    }

    PetManager   pm("pets.txt");
    TabBar       tabs(font);
    Toast        toast;
    Form         form(font);
    PetsView     petsView(pm, font, toast, form);
    CalendarView calView(pm, font, toast);
    ScheduleView schView(pm, font, toast); 
    StatsView    statsView(font);
    SettingsView setView(font,toast);

    sf::Clock clock;
    while (win.isOpen()) {
        sf::Event e;
        while (win.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                win.close();
                break;
            }
  
            sf::Vector2f mp = (sf::Vector2f)sf::Mouse::getPosition(win);
            tabs.handleClick(mp);

            switch (tabs.selTab()) {
            case TabBar::Pets:
                petsView.handleEvent(e, win);
                break;
            case TabBar::Calendar:
                calView.handleEvent(e, win);
                break;
            case TabBar::Schedule:
                schView.handleEvent(e, win);
                break;
            case TabBar::Stats:
                statsView.handleEvent(e, win);
                break;
            case TabBar::Settings:
                setView.handleEvent(e, win);
                break;
            }
        }

        float dt = clock.restart().asSeconds();
        tabs.update(dt);
        form.update(dt);
        petsView.update(dt);
        calView.update(dt);
        schView.update(dt);
        statsView.update(dt);
        setView.update(dt);

        bool darkBg = (tabs.selTab() == TabBar::Settings);
        win.clear(darkBg ? sf::Color(30, 30, 40) : sf::Color(0, 105, 148));

        switch (tabs.selTab()) {
        case TabBar::Pets:     petsView.draw(win);   break;
        case TabBar::Calendar: calView.draw(win);    break;
        case TabBar::Schedule: schView.draw(win);    break;
        case TabBar::Stats:    statsView.draw(win);  break;
        case TabBar::Settings: setView.draw(win);    break;
        }

        tabs.draw(win);
        form.draw(win);
        toast.draw(win);

        win.display();
    }

    return 0;
}
