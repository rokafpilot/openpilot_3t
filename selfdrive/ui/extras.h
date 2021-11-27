#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>

#pragma once

#include <vector>
#include <utility>

class ATextItem {
  public:
    std::string text;
    int alpha;

    ATextItem(const char* text, int alpha) {
      this->text = text;
      this->alpha = alpha;
    }
};

class AText {
  private:
    std::string font_name;
    std::vector<ATextItem> items;
    std::string last_text;

  public:

    AText(const char *font_name) {
      this->font_name = font_name;
    }

    void update(const UIState *s, float x, float y, const char *string, int size, NVGcolor color) {
      if(last_text != string) {
        this->items.insert(this->items.begin(), std::move(ATextItem(string, 255)));
        last_text = string;
      }

      for(auto it = this->items.begin() + 1; it != this->items.end();)  {
        it->alpha -= 1000 / UI_FREQ;
        if(it->alpha <= 0)
          it = this->items.erase(it);
        else
          it++;
      }

      nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);

      for(auto it = this->items.rbegin(); it != this->items.rend(); ++it)  {
        color.a = it->alpha/255.f;
        ui_draw_text(s, x, y, it->text.c_str(), size, color, this->font_name.c_str());
      }
    }

    void ui_draw_text(const UIState *s, float x, float y, const char *string, float size, NVGcolor color, const char *font_name) {
      nvgFontFace(s->vg, font_name);
      nvgFontSize(s->vg, size);
      nvgFillColor(s->vg, color);
      nvgText(s->vg, x, y, string, NULL);
    }
};

static void ui_draw_extras_limit_speed(UIState *s)
{
    const UIScene *scene = &s->scene;
    cereal::CarControl::SccSmoother::Reader scc_smoother = scene->car_control.getSccSmoother();
    int activeNDA = scc_smoother.getRoadLimitSpeedActive();
    int limit_speed = scc_smoother.getRoadLimitSpeed();
    int left_dist = scc_smoother.getRoadLimitSpeedLeftDist();

    if(activeNDA > 0)
    {
        int w = 120;
        int h = 54;
        int x = (s->fb_w + (bdr_s*2))/2 - w/2 - bdr_s;
        int y = 40 - bdr_s;

        const char* img = activeNDA == 1 ? "img_nda" : "img_hda";
        ui_draw_image(s, {x, y, w, h}, img, 1.f);
    }

    if(limit_speed > 10 && left_dist > 0)
    {
        int w = s->fb_w / 10;
        int h = s->fb_w / 10;
        int x = (bdr_s*2) + 220 + s->fb_w / 25;
        int y = 100;
        char str[32];

        nvgBeginPath(s->vg);
        nvgRoundedRect(s->vg, x, y, w, h, s->fb_w / 9);
        nvgStrokeColor(s->vg, nvgRGBA(255, 0, 0, 200));
        nvgStrokeWidth(s->vg, s->fb_w / 72);
        nvgStroke(s->vg);

        NVGcolor fillColor = nvgRGBA(0, 0, 0, 50);
        nvgFillColor(s->vg, fillColor);
        nvgFill(s->vg);

        nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 250));

        nvgFontSize(s->vg, s->fb_w / 15);
        nvgFontFace(s->vg, "sans-bold");
        nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        snprintf(str, sizeof(str), "%d", limit_speed);
        nvgText(s->vg, x+w/2, y+h/2, str, NULL);

        nvgFontSize(s->vg, s->fb_w / 18);

        if(left_dist >= 1000)
            snprintf(str, sizeof(str), "%.1fkm", left_dist / 1000.f);
        else
            snprintf(str, sizeof(str), "%dm", left_dist);

        nvgText(s->vg, x+w/2, y+h + 70, str, NULL);
    }
    else
    {
        auto controls_state = (*s->sm)["controlsState"].getControlsState();
        int sccStockCamAct = (int)controls_state.getSccStockCamAct();
        int sccStockCamStatus = (int)controls_state.getSccStockCamStatus();

        if(sccStockCamAct == 2 && sccStockCamStatus == 2)
        {
            int w = s->fb_w / 10;
            int h = s->fb_w / 10;
            int x = (bdr_s*2) + 220 + s->fb_w / 25;
            int y = 100;
            char str[32];

            nvgBeginPath(s->vg);
            nvgRoundedRect(s->vg, x, y, w, h, s->fb_w / 9);
            nvgStrokeColor(s->vg, nvgRGBA(255, 0, 0, 200));
            nvgStrokeWidth(s->vg, s->fb_w / 72);
            nvgStroke(s->vg);

            NVGcolor fillColor = nvgRGBA(0, 0, 0, 50);
            nvgFillColor(s->vg, fillColor);
            nvgFill(s->vg);

            nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 250));

            nvgFontSize(s->vg, s->fb_w / 15);
            nvgFontFace(s->vg, "sans-bold");
            nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

            nvgText(s->vg, x+w/2, y+h/2, "CAM", NULL);
        }
    }
}

static NVGcolor get_tpms_color(float tpms) {
    if(tpms < 5 || tpms > 60) // N/A
        return nvgRGBA(255, 255, 255, 200);
    if(tpms < 31)
        return nvgRGBA(255, 90, 90, 200);
    return nvgRGBA(255, 255, 255, 200);
}

static std::string get_tpms_text(float tpms) {
    if(tpms < 5 || tpms > 60)
        return "";

    char str[32];
    snprintf(str, sizeof(str), "%.0f", round(tpms));
    return std::string(str);
}

static void ui_draw_extras_tire_pressure(UIState *s)
{
    const UIScene *scene = &s->scene;
    auto car_state = (*s->sm)["carState"].getCarState();
    auto tpms = car_state.getTpms();

    const float fl = tpms.getFl();
    const float fr = tpms.getFr();
    const float rl = tpms.getRl();
    const float rr = tpms.getRr();

    const int w = 58;
    const int h = 126;

    const int radius = 96;
    const int x = ((radius / 2) + (bdr_s * 2)) * 3;
    const int y = s->fb_h - bdr_s - h - 50;
    const int margin = 10;

    const int rect_x = radius * 2 + 5;
    const int rect_y = s->fb_h - bdr_s - h - 60;
    const int rect_w = radius * 2;
    const int rect_h = radius * 1.5;

    nvgBeginPath(s->vg);
    ui_draw_image(s, {x, y, w, h}, "tire_pressure", 0.8f);

    nvgFontSize(s->vg, 60);
    nvgFontFace(s->vg, "sans-semibold");

    nvgTextAlign(s->vg, NVG_ALIGN_RIGHT);
    nvgFillColor(s->vg, get_tpms_color(fl));
    nvgText(s->vg, x-margin, y+45, get_tpms_text(fl).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_LEFT);
    nvgFillColor(s->vg, get_tpms_color(fr));
    nvgText(s->vg, x+w+margin, y+45, get_tpms_text(fr).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_RIGHT);
    nvgFillColor(s->vg, get_tpms_color(rl));
    nvgText(s->vg, x-margin, y+h-15, get_tpms_text(rl).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_LEFT);
    nvgFillColor(s->vg, get_tpms_color(rr));
    nvgText(s->vg, x+w+margin, y+h-15, get_tpms_text(rr).c_str(), NULL);

    // drwa frame
    NVGcolor color_bg = nvgRGBA(0, 0, 0, (255 * 0.15f));

    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, rect_x, rect_y, rect_w, rect_h, 20);
    nvgFillColor(s->vg, color_bg);
    nvgFill(s->vg);
    nvgStrokeColor(s->vg, nvgRGBA(255, 255, 255, 80));
    nvgStrokeWidth(s->vg, 3);
    nvgStroke(s->vg);
}

static void ui_draw_extras(UIState *s)
{
    ui_draw_extras_limit_speed(s);
    ui_draw_extras_tire_pressure(s);
}
