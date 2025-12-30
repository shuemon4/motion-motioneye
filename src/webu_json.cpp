/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
*/

#include "motion.hpp"
#include "util.hpp"
#include "camera.hpp"
#include "conf.hpp"
#include "logger.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_json.hpp"
#include "dbse.hpp"
#include "libcam.hpp"
#include "json_parse.hpp"
#include <map>
#include <algorithm>
#include <vector>
#include <thread>
#include <sys/statvfs.h>

/* CPU-efficient polygon fill using scanline algorithm
 * Fills polygon interior with specified value in bitmap
 * O(height * edges) complexity, minimal memory allocation
 */
static void fill_polygon(u_char *bitmap, int width, int height,
    const std::vector<std::pair<int,int>> &polygon, u_char fill_val)
{
    if (polygon.size() < 3) return;

    /* Find vertical bounds */
    int min_y = height, max_y = 0;
    for (const auto &pt : polygon) {
        if (pt.second < min_y) min_y = pt.second;
        if (pt.second > max_y) max_y = pt.second;
    }

    /* Clamp to image bounds */
    if (min_y < 0) min_y = 0;
    if (max_y >= height) max_y = height - 1;

    /* Scanline fill */
    std::vector<int> x_intersects;
    for (int y = min_y; y <= max_y; y++) {
        x_intersects.clear();

        /* Find intersections with polygon edges */
        size_t n = polygon.size();
        for (size_t i = 0; i < n; i++) {
            int x1 = polygon[i].first;
            int y1 = polygon[i].second;
            int x2 = polygon[(i + 1) % n].first;
            int y2 = polygon[(i + 1) % n].second;

            /* Check if edge crosses this scanline */
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                /* Compute x intersection using integer math to avoid float */
                int x = x1 + ((y - y1) * (x2 - x1)) / (y2 - y1);
                x_intersects.push_back(x);
            }
        }

        /* Sort intersections */
        std::sort(x_intersects.begin(), x_intersects.end());

        /* Fill between pairs */
        for (size_t i = 0; i + 1 < x_intersects.size(); i += 2) {
            int xs = x_intersects[i];
            int xe = x_intersects[i + 1];

            /* Clamp to image bounds */
            if (xs < 0) xs = 0;
            if (xe >= width) xe = width - 1;

            /* Fill the span */
            for (int x = xs; x <= xe; x++) {
                bitmap[y * width + x] = fill_val;
            }
        }
    }
}

/* Generate auto-path for mask file in target_dir */
static std::string build_mask_path(cls_camera *cam, const std::string &type)
{
    std::string target = cam->cfg->target_dir;
    if (target.empty()) {
        target = "/var/lib/motion";
    }
    /* Remove trailing slash */
    if (!target.empty() && target.back() == '/') {
        target.pop_back();
    }
    return target + "/cam" + std::to_string(cam->cfg->device_id) +
           "_" + type + ".pgm";
}

std::string cls_webu_json::escstr(std::string invar)
{
    std::string  outvar;
    size_t indx;
    for (indx = 0; indx <invar.length(); indx++) {
        if (invar[indx] == '\\' ||
            invar[indx] == '\"') {
                outvar += '\\';
            }
        outvar += invar[indx];
    }
    return outvar;
}

void cls_webu_json::parms_item_detail(cls_config *conf, std::string pNm)
{
    ctx_params  *params;
    ctx_params_item *itm;
    int indx;

    params = new ctx_params;
    params->params_cnt = 0;
    mylower(pNm);

    if (pNm == "v4l2_params") {
        util_parms_parse(params, pNm, conf->v4l2_params);
    } else if (pNm == "netcam_params") {
        util_parms_parse(params, pNm, conf->netcam_params);
    } else if (pNm == "netcam_high_params") {
        util_parms_parse(params, pNm, conf->netcam_high_params);
    } else if (pNm == "libcam_params") {
        util_parms_parse(params, pNm, conf->libcam_params);
    } else if (pNm == "schedule_params") {
        util_parms_parse(params, pNm, conf->schedule_params);
    } else if (pNm == "cleandir_params") {
        util_parms_parse(params, pNm, conf->cleandir_params);
    } else if (pNm == "secondary_params") {
        util_parms_parse(params, pNm, conf->secondary_params);
    } else if (pNm == "webcontrol_actions") {
        util_parms_parse(params, pNm, conf->webcontrol_actions);
    } else if (pNm == "webcontrol_headers") {
        util_parms_parse(params, pNm, conf->webcontrol_headers);
    } else if (pNm == "stream_preview_params") {
        util_parms_parse(params, pNm, conf->stream_preview_params);
    } else if (pNm == "snd_params") {
        util_parms_parse(params, pNm, conf->snd_params);
    }

    webua->resp_page += ",\"count\":";
    webua->resp_page += std::to_string(params->params_cnt);

    if (params->params_cnt > 0) {
        webua->resp_page += ",\"parsed\" :{";
        for (indx=0; indx<params->params_cnt; indx++) {
            itm = &params->params_array[indx];
            if (indx != 0) {
                webua->resp_page += ",";
            }
            webua->resp_page += "\""+std::to_string(indx)+"\":";
            webua->resp_page += "{\"name\":\""+itm->param_name+"\",";
            webua->resp_page += "\"value\":\""+itm->param_value+"\"}";

        }
        webua->resp_page += "}";
    }

    mydelete(params);

}

void cls_webu_json::parms_item(cls_config *conf, int indx_parm)
{
    std::string parm_orig, parm_val, parm_list, parm_enable;

    parm_orig = "";
    parm_val = "";
    parm_list = "[]";  // Default to empty JSON array for valid JSON

    if (app->cfg->webcontrol_parms < PARM_LEVEL_LIMITED) {
        parm_enable = "false";
    } else {
        parm_enable = "true";
    }

    conf->edit_get(config_parms[indx_parm].parm_name
        , parm_orig, config_parms[indx_parm].parm_cat);

    parm_val = escstr(parm_orig);

    if (config_parms[indx_parm].parm_type == PARM_TYP_INT) {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":" + parm_val +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";

    } else if (config_parms[indx_parm].parm_type == PARM_TYP_BOOL) {
        if (parm_val == "on") {
            webua->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":true" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\""+
                "}";
        } else {
            webua->resp_page +=
                "\"" + config_parms[indx_parm].parm_name + "\"" +
                ":{" +
                " \"value\":false" +
                ",\"enabled\":" + parm_enable +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
                "}";
        }
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
        conf->edit_list(config_parms[indx_parm].parm_name
            , parm_list, config_parms[indx_parm].parm_cat);
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\": \"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\"" + conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            ",\"list\":" + parm_list +
            "}";
    } else if (config_parms[indx_parm].parm_type == PARM_TYP_PARAMS) {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"";
        parms_item_detail(conf, config_parms[indx_parm].parm_name);
        webua->resp_page += "}";
    } else {
        webua->resp_page +=
            "\"" + config_parms[indx_parm].parm_name + "\"" +
            ":{" +
            " \"value\":\"" + parm_val + "\"" +
            ",\"enabled\":" + parm_enable +
            ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
            ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"" +
            "}";
    }
}

void cls_webu_json::parms_one(cls_config *conf)
{
    int indx_parm;
    bool first;
    std::string response;

    indx_parm = 0;
    first = true;
    while ((config_parms[indx_parm].parm_name != "") ) {
        if (config_parms[indx_parm].webui_level == PARM_LEVEL_NEVER) {
            indx_parm++;
            continue;
        }
        if (first) {
            first = false;
            webua->resp_page += "{";
        } else {
            webua->resp_page += ",";
        }
        /* Allow limited parameters to be read only to the web page */
        if ((config_parms[indx_parm].webui_level >
                app->cfg->webcontrol_parms) &&
            (config_parms[indx_parm].webui_level > PARM_LEVEL_LIMITED)) {

            webua->resp_page +=
                "\""+config_parms[indx_parm].parm_name+"\"" +
                ":{" +
                " \"value\":\"\"" +
                ",\"enabled\":false" +
                ",\"category\":" + std::to_string(config_parms[indx_parm].parm_cat) +
                ",\"type\":\""+ conf->type_desc(config_parms[indx_parm].parm_type) + "\"";

            if (config_parms[indx_parm].parm_type == PARM_TYP_LIST) {
                webua->resp_page += ",\"list\":[\"na\"]";
            }
            webua->resp_page +="}";
        } else {
           parms_item(conf, indx_parm);
        }
        indx_parm++;
    }
    webua->resp_page += "}";
}

void cls_webu_json::parms_all()
{
    int indx_cam;

    webua->resp_page += "{";
    webua->resp_page += "\"default\": ";
    parms_one(app->cfg);

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        webua->resp_page += ",\"cam" +
            std::to_string(app->cam_list[indx_cam]->cfg->device_id) + "\": ";
        parms_one(app->cam_list[indx_cam]->cfg);
    }
    webua->resp_page += "}";
}

void cls_webu_json::cameras_list()
{
    int indx_cam;
    std::string response;
    std::string strid;
    cls_camera     *cam;

    webua->resp_page += "{\"count\" : " + std::to_string(app->cam_cnt);

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        cam = app->cam_list[indx_cam];
        strid =std::to_string(cam->cfg->device_id);
        webua->resp_page += ",\"" + std::to_string(indx_cam) + "\":";
        if (cam->cfg->device_name == "") {
            webua->resp_page += "{\"name\": \"camera " + strid + "\"";
        } else {
            webua->resp_page += "{\"name\": \"" + escstr(cam->cfg->device_name) + "\"";
        }
        webua->resp_page += ",\"id\": " + strid;
        webua->resp_page += ",\"all_xpct_st\": " + std::to_string(cam->all_loc.xpct_st);
        webua->resp_page += ",\"all_xpct_en\": " + std::to_string(cam->all_loc.xpct_en);
        webua->resp_page += ",\"all_ypct_st\": " + std::to_string(cam->all_loc.ypct_st);
        webua->resp_page += ",\"all_ypct_en\": " + std::to_string(cam->all_loc.ypct_en);
        webua->resp_page += ",\"url\": \"" + webua->hostfull + "/" + strid + "/\"} ";
    }
    webua->resp_page += "}";

}

void cls_webu_json::categories_list()
{
    int indx_cat;
    std::string catnm_short, catnm_long;

    webua->resp_page += "{";

    indx_cat = 0;
    while (indx_cat != PARM_CAT_MAX) {
        if (indx_cat != 0) {
            webua->resp_page += ",";
        }
        webua->resp_page += "\"" + std::to_string(indx_cat) + "\": ";

        catnm_long = webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, false);
        catnm_short = webua->app->cfg->cat_desc((enum PARM_CAT)indx_cat, true);

        webua->resp_page += "{\"name\":\"" + catnm_short + "\",\"display\":\"" + catnm_long + "\"}";

        indx_cat++;
    }

    webua->resp_page += "}";
}

void cls_webu_json::config()
{
    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"version\" : \"" VERSION "\"";

    webua->resp_page += ",\"cameras\" : ";
    cameras_list();

    webua->resp_page += ",\"configuration\" : ";
    parms_all();

    webua->resp_page += ",\"categories\" : ";
    categories_list();

    webua->resp_page += "}";
}

void cls_webu_json::movies_list()
{
    int indx, indx2;
    std::string response;
    char fmt[PATH_MAX];
    vec_files flst;
    std::string sql;

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "movies") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webua->resp_page += "{\"count\" : 0} ";
                webua->resp_page += ",\"device_id\" : ";
                webua->resp_page += std::to_string(webua->cam->cfg->device_id);
                webua->resp_page += "}";
                return;
            } else {
                break;
            }
        }
    }

    sql  = " select * from motion ";
    sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " order by file_dtl, file_tml;";
    app->dbse->filelist_get(sql, flst);

    webua->resp_page += "{";
    indx = 0;
    for (indx2=0;indx2<flst.size();indx2++){
        if (flst[indx2].found == true) {
            if ((flst[indx2].file_sz/1000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fKB"
                    ,((double)flst[indx2].file_sz/1000));
            } else if ((flst[indx2].file_sz/1000000) < 1000) {
                snprintf(fmt,PATH_MAX,"%.1fMB"
                    ,((double)flst[indx2].file_sz/1000000));
            } else {
                snprintf(fmt,PATH_MAX,"%.1fGB"
                    ,((double)flst[indx2].file_sz/1000000000));
            }
            webua->resp_page += "\""+ std::to_string(indx) + "\":";

            webua->resp_page += "{\"name\": \"";
            webua->resp_page += escstr(flst[indx2].file_nm) + "\"";

            webua->resp_page += ",\"size\": \"";
            webua->resp_page += std::string(fmt) + "\"";

            webua->resp_page += ",\"date\": \"";
            webua->resp_page += std::to_string(flst[indx2].file_dtl) + "\"";

            webua->resp_page += ",\"time\": \"";
            webua->resp_page += flst[indx2].file_tmc + "\"";

            webua->resp_page += ",\"diff_avg\": \"";
            webua->resp_page += std::to_string(flst[indx2].diff_avg) + "\"";

            webua->resp_page += ",\"sdev_min\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_min) + "\"";

            webua->resp_page += ",\"sdev_max\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_max) + "\"";

            webua->resp_page += ",\"sdev_avg\": \"";
            webua->resp_page += std::to_string(flst[indx2].sdev_avg) + "\"";

            webua->resp_page += "}";
            webua->resp_page += ",";
            indx++;
        }
    }
    webua->resp_page += "\"count\" : " + std::to_string(indx);
    webua->resp_page += ",\"device_id\" : ";
    webua->resp_page += std::to_string(webua->cam->cfg->device_id);
    webua->resp_page += "}";
}

void cls_webu_json::movies()
{
    int indx_cam, indx_req;

    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"movies\" : ";
    if (webua->cam == NULL) {
        webua->resp_page += "{\"count\" :" + std::to_string(app->cam_cnt);

        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            webua->cam = app->cam_list[indx_cam];
            webua->resp_page += ",\""+ std::to_string(indx_cam) + "\":";
            movies_list();
        }
        webua->resp_page += "}";
        webua->cam = NULL;
    } else {
        indx_req = -1;
        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            if (webua->cam->cfg->device_id == app->cam_list[indx_cam]->cfg->device_id){
                indx_req = indx_cam;
            }
        }
        webua->resp_page += "{\"count\" : 1";
        webua->resp_page += ",\""+ std::to_string(indx_req) + "\":";
        movies_list();
        webua->resp_page += "}";
    }
    webua->resp_page += "}";
}

void cls_webu_json::status_vars(int indx_cam)
{
    char buf[32];
    struct tm timestamp_tm;
    struct timespec curr_ts;
    cls_camera *cam;

    cam = app->cam_list[indx_cam];

    webua->resp_page += "{";

    webua->resp_page += "\"name\":\"" + escstr(cam->cfg->device_name)+"\"";
    webua->resp_page += ",\"id\":" + std::to_string(cam->cfg->device_id);
    webua->resp_page += ",\"width\":" + std::to_string(cam->imgs.width);
    webua->resp_page += ",\"height\":" + std::to_string(cam->imgs.height);
    webua->resp_page += ",\"fps\":" + std::to_string(cam->lastrate);

    clock_gettime(CLOCK_REALTIME, &curr_ts);
    localtime_r(&curr_ts.tv_sec, &timestamp_tm);
    strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
    webua->resp_page += ",\"current_time\":\"" + std::string(buf)+"\"";

    webua->resp_page += ",\"missing_frame_counter\":" +
        std::to_string(cam->missing_frame_counter);

    if (cam->lost_connection) {
        webua->resp_page += ",\"lost_connection\":true";
    } else {
        webua->resp_page += ",\"lost_connection\":false";
    }

    if (cam->connectionlosttime.tv_sec != 0) {
        localtime_r(&cam->connectionlosttime.tv_sec, &timestamp_tm);
        strftime(buf, sizeof(buf), "%FT%T", &timestamp_tm);
        webua->resp_page += ",\"connection_lost_time\":\"" + std::string(buf)+"\"";
    } else {
        webua->resp_page += ",\"connection_lost_time\":\"\"" ;
    }
    if (cam->detecting_motion) {
        webua->resp_page += ",\"detecting\":true";
    } else {
        webua->resp_page += ",\"detecting\":false";
    }

    if (cam->pause) {
        webua->resp_page += ",\"pause\":true";
    } else {
        webua->resp_page += ",\"pause\":false";
    }

    webua->resp_page += ",\"user_pause\":\"" + cam->user_pause +"\"";

    /* Add supportedControls for libcamera capability discovery */
    #ifdef HAVE_LIBCAM
    if (cam->has_libcam()) {
        webua->resp_page += ",\"supportedControls\":{";
        std::map<std::string, bool> caps = cam->get_libcam_capabilities();
        bool first = true;
        for (const auto& [name, supported] : caps) {
            if (!first) {
                webua->resp_page += ",";
            }
            webua->resp_page += "\"" + name + "\":" +
                               (supported ? "true" : "false");
            first = false;
        }
        webua->resp_page += "}";
    }
    #endif

    webua->resp_page += "}";
}

void cls_webu_json::status()
{
    int indx_cam;

    webua->resp_type = WEBUI_RESP_JSON;

    webua->resp_page += "{\"version\" : \"" VERSION "\"";
    webua->resp_page += ",\"status\" : ";

    webua->resp_page += "{\"count\" : " + std::to_string(app->cam_cnt);
        for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
            webua->resp_page += ",\"cam" +
                std::to_string(app->cam_list[indx_cam]->cfg->device_id) + "\": ";
            status_vars(indx_cam);
        }
    webua->resp_page += "}";

    webua->resp_page += "}";
}

void cls_webu_json::loghistory()
{
    int indx, cnt;
    bool frst;

    webua->resp_type = WEBUI_RESP_JSON;
    webua->resp_page = "";

    frst = true;
    cnt = 0;

    pthread_mutex_lock(&motlog->mutex_log);
        for (indx=0; indx<motlog->log_vec.size();indx++) {
            if (motlog->log_vec[indx].log_nbr > mtoi(webua->uri_cmd2)) {
                if (frst == true) {
                    webua->resp_page += "{";
                    frst = false;
                } else {
                    webua->resp_page += ",";
                }
                webua->resp_page += "\"" + std::to_string(indx) +"\" : {";
                webua->resp_page += "\"lognbr\" :\"" +
                    std::to_string(motlog->log_vec[indx].log_nbr) + "\", ";
                webua->resp_page += "\"logmsg\" :\"" +
                    escstr(motlog->log_vec[indx].log_msg.substr(0,
                        motlog->log_vec[indx].log_msg.length()-1)) + "\" ";
                webua->resp_page += "}";
                cnt++;
            }
        }
    pthread_mutex_unlock(&motlog->mutex_log);
    if (frst == true) {
        webua->resp_page += "{\"0\":\"\" ";
    }
    webua->resp_page += ",\"count\":\""+std::to_string(cnt)+"\"}";

}

/*
 * Hot Reload API: Validate parameter exists and is hot-reloadable
 * Returns true if parameter can be hot-reloaded
 * Sets parm_index to the index in config_parms[] (-1 if not found)
 */
bool cls_webu_json::validate_hot_reload(const std::string &parm_name, int &parm_index)
{
    parm_index = 0;
    while (config_parms[parm_index].parm_name != "") {
        if (config_parms[parm_index].parm_name == parm_name) {
            /* Check permission level */
            if (config_parms[parm_index].webui_level > app->cfg->webcontrol_parms) {
                return false;
            }
            /* Check hot reload flag */
            return config_parms[parm_index].hot_reload;
        }
        parm_index++;
    }
    parm_index = -1;  /* Not found */
    return false;
}

/*
 * Hot Reload API: Apply parameter change to config
 */
void cls_webu_json::apply_hot_reload(int parm_index, const std::string &parm_val)
{
    std::string parm_name = config_parms[parm_index].parm_name;

    if (webua->device_id == 0) {
        /* Update default config */
        app->cfg->edit_set(parm_name, parm_val);

        /* Update all running cameras */
        for (int indx = 0; indx < app->cam_cnt; indx++) {
            app->cam_list[indx]->cfg->edit_set(parm_name, parm_val);

            /* Apply libcam brightness/contrast/ISO/AWB changes immediately */
            if (parm_name == "libcam_brightness") {
                app->cam_list[indx]->set_libcam_brightness(atof(parm_val.c_str()));
            } else if (parm_name == "libcam_contrast") {
                app->cam_list[indx]->set_libcam_contrast(atof(parm_val.c_str()));
            } else if (parm_name == "libcam_iso") {
                app->cam_list[indx]->set_libcam_iso(atof(parm_val.c_str()));
            } else if (parm_name == "libcam_awb_enable") {
                app->cam_list[indx]->set_libcam_awb_enable(parm_val == "true" || parm_val == "1");
            } else if (parm_name == "libcam_awb_mode") {
                app->cam_list[indx]->set_libcam_awb_mode(atoi(parm_val.c_str()));
            } else if (parm_name == "libcam_awb_locked") {
                app->cam_list[indx]->set_libcam_awb_locked(parm_val == "true" || parm_val == "1");
            } else if (parm_name == "libcam_colour_temp") {
                app->cam_list[indx]->set_libcam_colour_temp(atoi(parm_val.c_str()));
            } else if (parm_name == "libcam_colour_gain_r") {
                float r = atof(parm_val.c_str());
                float b = app->cam_list[indx]->cfg->parm_cam.libcam_colour_gain_b;
                app->cam_list[indx]->set_libcam_colour_gains(r, b);
            } else if (parm_name == "libcam_colour_gain_b") {
                float r = app->cam_list[indx]->cfg->parm_cam.libcam_colour_gain_r;
                float b = atof(parm_val.c_str());
                app->cam_list[indx]->set_libcam_colour_gains(r, b);
            } else if (parm_name == "libcam_af_mode") {
                app->cam_list[indx]->set_libcam_af_mode(atoi(parm_val.c_str()));
            } else if (parm_name == "libcam_lens_position") {
                app->cam_list[indx]->set_libcam_lens_position(atof(parm_val.c_str()));
            } else if (parm_name == "libcam_af_range") {
                app->cam_list[indx]->set_libcam_af_range(atoi(parm_val.c_str()));
            } else if (parm_name == "libcam_af_speed") {
                app->cam_list[indx]->set_libcam_af_speed(atoi(parm_val.c_str()));
            } else if (parm_name == "libcam_af_trigger") {
                int val = atoi(parm_val.c_str());
                if (val == 0) {
                    app->cam_list[indx]->trigger_libcam_af_scan();
                } else {
                    app->cam_list[indx]->cancel_libcam_af_scan();
                }
            }
        }
    } else if (webua->cam != nullptr) {
        /* Update specific camera only */
        webua->cam->cfg->edit_set(parm_name, parm_val);

        /* Apply libcam brightness/contrast/ISO/AWB changes immediately */
        if (parm_name == "libcam_brightness") {
            webua->cam->set_libcam_brightness(atof(parm_val.c_str()));
        } else if (parm_name == "libcam_contrast") {
            webua->cam->set_libcam_contrast(atof(parm_val.c_str()));
        } else if (parm_name == "libcam_iso") {
            webua->cam->set_libcam_iso(atof(parm_val.c_str()));
        } else if (parm_name == "libcam_awb_enable") {
            webua->cam->set_libcam_awb_enable(parm_val == "true" || parm_val == "1");
        } else if (parm_name == "libcam_awb_mode") {
            webua->cam->set_libcam_awb_mode(atoi(parm_val.c_str()));
        } else if (parm_name == "libcam_awb_locked") {
            webua->cam->set_libcam_awb_locked(parm_val == "true" || parm_val == "1");
        } else if (parm_name == "libcam_colour_temp") {
            webua->cam->set_libcam_colour_temp(atoi(parm_val.c_str()));
        } else if (parm_name == "libcam_colour_gain_r") {
            float r = atof(parm_val.c_str());
            float b = webua->cam->cfg->parm_cam.libcam_colour_gain_b;
            webua->cam->set_libcam_colour_gains(r, b);
        } else if (parm_name == "libcam_colour_gain_b") {
            float r = webua->cam->cfg->parm_cam.libcam_colour_gain_r;
            float b = atof(parm_val.c_str());
            webua->cam->set_libcam_colour_gains(r, b);
        } else if (parm_name == "libcam_af_mode") {
            webua->cam->set_libcam_af_mode(atoi(parm_val.c_str()));
        } else if (parm_name == "libcam_lens_position") {
            webua->cam->set_libcam_lens_position(atof(parm_val.c_str()));
        } else if (parm_name == "libcam_af_range") {
            webua->cam->set_libcam_af_range(atoi(parm_val.c_str()));
        } else if (parm_name == "libcam_af_speed") {
            webua->cam->set_libcam_af_speed(atoi(parm_val.c_str()));
        } else if (parm_name == "libcam_af_trigger") {
            int val = atoi(parm_val.c_str());
            if (val == 0) {
                webua->cam->trigger_libcam_af_scan();
            } else {
                webua->cam->cancel_libcam_af_scan();
            }
        }
    }

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Hot reload: %s = %s (camera %d)",
        parm_name.c_str(), parm_val.c_str(), webua->device_id);
}


/*
 * React UI API: Authentication status
 * Returns current authentication state
 */
void cls_webu_json::api_auth_me()
{
    webua->resp_page = "{";

    /* Check if authentication is configured */
    if (app->cfg->webcontrol_authentication != "") {
        webua->resp_page += "\"authenticated\":true,";
        webua->resp_page += "\"auth_method\":\"digest\"";
    } else {
        webua->resp_page += "\"authenticated\":false";
    }

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Media pictures list
 * Returns list of snapshot images for a camera
 */
void cls_webu_json::api_media_pictures()
{
    vec_files flst;
    std::string sql;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    sql  = " select * from motion ";
    sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = '1'";  /* 1 = snapshot */
    sql += " order by file_dtl desc, file_tml desc";
    sql += " limit 100;";

    app->dbse->filelist_get(sql, flst);

    webua->resp_page = "{\"pictures\":[";
    for (size_t i = 0; i < flst.size(); i++) {
        if (i > 0) webua->resp_page += ",";
        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + std::to_string(flst[i].record_id) + ",";
        webua->resp_page += "\"filename\":\"" + escstr(flst[i].file_nm) + "\",";
        webua->resp_page += "\"path\":\"" + escstr(flst[i].full_nm) + "\",";
        webua->resp_page += "\"date\":\"" + std::to_string(flst[i].file_dtl) + "\",";
        webua->resp_page += "\"time\":\"" + escstr(flst[i].file_tml) + "\",";
        webua->resp_page += "\"size\":" + std::to_string(flst[i].file_sz);
        webua->resp_page += "}";
    }
    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Delete a picture file
 * DELETE /{camId}/api/media/picture/{id}
 * Deletes both the file and database record
 */
void cls_webu_json::api_delete_picture()
{
    int indx;
    std::string sql, full_path;
    vec_files flst;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Check if delete action is enabled */
    for (indx=0; indx<webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Delete action disabled");
                webua->resp_page = "{\"error\":\"Delete action is disabled\"}";
                webua->resp_type = WEBUI_RESP_JSON;
                return;
            }
            break;
        }
    }

    /* Get file ID from URI: uri_cmd4 contains the record ID */
    if (webua->uri_cmd4.empty()) {
        webua->resp_page = "{\"error\":\"File ID required\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    int file_id = mtoi(webua->uri_cmd4);
    if (file_id <= 0) {
        webua->resp_page = "{\"error\":\"Invalid file ID\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Look up the file in database */
    sql  = " select * from motion ";
    sql += " where record_id = " + std::to_string(file_id);
    sql += " and device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = '1'";  /* 1 = picture */
    app->dbse->filelist_get(sql, flst);

    if (flst.empty()) {
        webua->resp_page = "{\"error\":\"File not found\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Security: Validate file path to prevent directory traversal */
    full_path = flst[0].full_nm;
    if (full_path.find("..") != std::string::npos) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            full_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid file path\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete the file from filesystem */
    if (remove(full_path.c_str()) != 0 && errno != ENOENT) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Failed to delete file: %s"), full_path.c_str());
        webua->resp_page = "{\"error\":\"Failed to delete file\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete from database */
    sql  = "delete from motion where record_id = " + std::to_string(file_id);
    app->dbse->exec_sql(sql);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Deleted picture: %s (id=%d) by %s",
        flst[0].file_nm.c_str(), file_id, webua->clientip.c_str());

    webua->resp_page = "{\"success\":true,\"deleted_id\":" + std::to_string(file_id) + "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Delete a movie file
 * DELETE /{camId}/api/media/movie/{id}
 * Deletes both the file and database record
 */
void cls_webu_json::api_delete_movie()
{
    int indx;
    std::string sql, full_path;
    vec_files flst;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Check if delete action is enabled */
    for (indx=0; indx<webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "delete") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Delete action disabled");
                webua->resp_page = "{\"error\":\"Delete action is disabled\"}";
                webua->resp_type = WEBUI_RESP_JSON;
                return;
            }
            break;
        }
    }

    /* Get file ID from URI: uri_cmd4 contains the record ID */
    if (webua->uri_cmd4.empty()) {
        webua->resp_page = "{\"error\":\"File ID required\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    int file_id = mtoi(webua->uri_cmd4);
    if (file_id <= 0) {
        webua->resp_page = "{\"error\":\"Invalid file ID\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Look up the file in database */
    sql  = " select * from motion ";
    sql += " where record_id = " + std::to_string(file_id);
    sql += " and device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = '2'";  /* 2 = movie */
    app->dbse->filelist_get(sql, flst);

    if (flst.empty()) {
        webua->resp_page = "{\"error\":\"File not found\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Security: Validate file path to prevent directory traversal */
    full_path = flst[0].full_nm;
    if (full_path.find("..") != std::string::npos) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s from %s"),
            full_path.c_str(), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Invalid file path\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete the file from filesystem */
    if (remove(full_path.c_str()) != 0 && errno != ENOENT) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("Failed to delete file: %s"), full_path.c_str());
        webua->resp_page = "{\"error\":\"Failed to delete file\"}";
        webua->resp_type = WEBUI_RESP_JSON;
        return;
    }

    /* Delete from database */
    sql  = "delete from motion where record_id = " + std::to_string(file_id);
    app->dbse->exec_sql(sql);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Deleted movie: %s (id=%d) by %s",
        flst[0].file_nm.c_str(), file_id, webua->clientip.c_str());

    webua->resp_page = "{\"success\":true,\"deleted_id\":" + std::to_string(file_id) + "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: List movies
 * Returns list of movie files from database
 */
void cls_webu_json::api_media_movies()
{
    vec_files flst;
    std::string sql;

    if (webua->cam == nullptr) {
        webua->bad_request();
        return;
    }

    sql  = " select * from motion ";
    sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
    sql += " and file_typ = '2'";  /* 2 = movie */
    sql += " order by file_dtl desc, file_tml desc";
    sql += " limit 100;";

    app->dbse->filelist_get(sql, flst);

    webua->resp_page = "{\"movies\":[";
    for (size_t i = 0; i < flst.size(); i++) {
        if (i > 0) webua->resp_page += ",";
        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + std::to_string(flst[i].record_id) + ",";
        webua->resp_page += "\"filename\":\"" + escstr(flst[i].file_nm) + "\",";
        webua->resp_page += "\"path\":\"" + escstr(flst[i].full_nm) + "\",";
        webua->resp_page += "\"date\":\"" + std::to_string(flst[i].file_dtl) + "\",";
        webua->resp_page += "\"time\":\"" + escstr(flst[i].file_tml) + "\",";
        webua->resp_page += "\"size\":" + std::to_string(flst[i].file_sz);
        webua->resp_page += "}";
    }
    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: System temperature
 * Returns CPU temperature (Raspberry Pi)
 */
void cls_webu_json::api_system_temperature()
{
    FILE *temp_file;
    int temp_raw;
    double temp_celsius;

    webua->resp_page = "{";

    temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp_file != nullptr) {
        if (fscanf(temp_file, "%d", &temp_raw) == 1) {
            temp_celsius = temp_raw / 1000.0;
            webua->resp_page += "\"celsius\":" + std::to_string(temp_celsius) + ",";
            webua->resp_page += "\"fahrenheit\":" + std::to_string(temp_celsius * 9.0 / 5.0 + 32.0);
        }
        fclose(temp_file);
    } else {
        webua->resp_page += "\"error\":\"Temperature not available\"";
    }

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: System status
 * Returns comprehensive system information (CPU temp, disk, memory, uptime)
 */
void cls_webu_json::api_system_status()
{
    FILE *file;
    char buffer[256];
    int temp_raw;
    double temp_celsius;
    unsigned long uptime_sec, mem_total, mem_free, mem_available;
    struct statvfs fs_stat;

    webua->resp_page = "{";

    /* CPU Temperature */
    file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (file != nullptr) {
        if (fscanf(file, "%d", &temp_raw) == 1) {
            temp_celsius = temp_raw / 1000.0;
            webua->resp_page += "\"temperature\":{";
            webua->resp_page += "\"celsius\":" + std::to_string(temp_celsius) + ",";
            webua->resp_page += "\"fahrenheit\":" + std::to_string(temp_celsius * 9.0 / 5.0 + 32.0);
            webua->resp_page += "},";
        }
        fclose(file);
    }

    /* System Uptime */
    file = fopen("/proc/uptime", "r");
    if (file != nullptr) {
        if (fscanf(file, "%lu", &uptime_sec) == 1) {
            webua->resp_page += "\"uptime\":{";
            webua->resp_page += "\"seconds\":" + std::to_string(uptime_sec) + ",";
            webua->resp_page += "\"days\":" + std::to_string(uptime_sec / 86400) + ",";
            webua->resp_page += "\"hours\":" + std::to_string((uptime_sec % 86400) / 3600);
            webua->resp_page += "},";
        }
        fclose(file);
    }

    /* Memory Information */
    file = fopen("/proc/meminfo", "r");
    if (file != nullptr) {
        mem_total = mem_free = mem_available = 0;
        while (fgets(buffer, sizeof(buffer), file)) {
            if (sscanf(buffer, "MemTotal: %lu kB", &mem_total) == 1) continue;
            if (sscanf(buffer, "MemFree: %lu kB", &mem_free) == 1) continue;
            if (sscanf(buffer, "MemAvailable: %lu kB", &mem_available) == 1) break;
        }
        fclose(file);

        if (mem_total > 0) {
            unsigned long mem_used = mem_total - mem_available;
            double mem_percent = (double)mem_used / mem_total * 100.0;
            webua->resp_page += "\"memory\":{";
            webua->resp_page += "\"total\":" + std::to_string(mem_total * 1024) + ",";
            webua->resp_page += "\"used\":" + std::to_string(mem_used * 1024) + ",";
            webua->resp_page += "\"free\":" + std::to_string(mem_free * 1024) + ",";
            webua->resp_page += "\"available\":" + std::to_string(mem_available * 1024) + ",";
            webua->resp_page += "\"percent\":" + std::to_string(mem_percent);
            webua->resp_page += "},";
        }
    }

    /* Disk Usage (root filesystem) */
    if (statvfs("/", &fs_stat) == 0) {
        unsigned long long total_bytes = (unsigned long long)fs_stat.f_blocks * fs_stat.f_frsize;
        unsigned long long free_bytes = (unsigned long long)fs_stat.f_bfree * fs_stat.f_frsize;
        unsigned long long avail_bytes = (unsigned long long)fs_stat.f_bavail * fs_stat.f_frsize;
        unsigned long long used_bytes = total_bytes - free_bytes;
        double disk_percent = (double)used_bytes / total_bytes * 100.0;

        webua->resp_page += "\"disk\":{";
        webua->resp_page += "\"total\":" + std::to_string(total_bytes) + ",";
        webua->resp_page += "\"used\":" + std::to_string(used_bytes) + ",";
        webua->resp_page += "\"free\":" + std::to_string(free_bytes) + ",";
        webua->resp_page += "\"available\":" + std::to_string(avail_bytes) + ",";
        webua->resp_page += "\"percent\":" + std::to_string(disk_percent);
        webua->resp_page += "},";
    }

    /* Motion Version */
    webua->resp_page += "\"version\":\"" + escstr(VERSION) + "\"";

    webua->resp_page += "}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: System reboot
 * POST /0/api/system/reboot
 * Requires CSRF token and authentication
 */
void cls_webu_json::api_system_reboot()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (csrf_token == nullptr || !webu->csrf_validate(std::string(csrf_token))) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for reboot from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Check if power control is enabled via webcontrol_actions */
    bool power_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "power") {
            if (webu->wb_actions->params_array[indx].param_value == "on") {
                power_enabled = true;
            }
            break;
        }
    }

    if (!power_enabled) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
            "Reboot request denied - power control disabled (from %s)", webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Power control is disabled\"}";
        return;
    }

    /* Log the reboot request */
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "System reboot requested by %s", webua->clientip.c_str());

    /* Schedule reboot with 2-second delay to allow HTTP response to complete */
    std::thread([]() {
        sleep(2);
        /* Try reboot commands in sequence (like MotionEye) */
        if (system("sudo /sbin/reboot") != 0) {
            if (system("sudo /sbin/shutdown -r now") != 0) {
                if (system("sudo /usr/bin/systemctl reboot") != 0) {
                    system("sudo /sbin/init 6");
                }
            }
        }
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"reboot\",\"message\":\"System will reboot in 2 seconds\"}";
}

/*
 * React UI API: System shutdown
 * POST /0/api/system/shutdown
 * Requires CSRF token and authentication
 */
void cls_webu_json::api_system_shutdown()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (csrf_token == nullptr || !webu->csrf_validate(std::string(csrf_token))) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for shutdown from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Check if power control is enabled via webcontrol_actions */
    bool power_enabled = false;
    for (int indx = 0; indx < webu->wb_actions->params_cnt; indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "power") {
            if (webu->wb_actions->params_array[indx].param_value == "on") {
                power_enabled = true;
            }
            break;
        }
    }

    if (!power_enabled) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
            "Shutdown request denied - power control disabled (from %s)", webua->clientip.c_str());
        webua->resp_page = "{\"error\":\"Power control is disabled\"}";
        return;
    }

    /* Log the shutdown request */
    MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO,
        "System shutdown requested by %s", webua->clientip.c_str());

    /* Schedule shutdown with 2-second delay to allow HTTP response to complete */
    std::thread([]() {
        sleep(2);
        /* Try shutdown commands in sequence (like MotionEye) */
        if (system("sudo /sbin/poweroff") != 0) {
            if (system("sudo /sbin/shutdown -h now") != 0) {
                if (system("sudo /usr/bin/systemctl poweroff") != 0) {
                    system("sudo /sbin/init 0");
                }
            }
        }
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"shutdown\",\"message\":\"System will shut down in 2 seconds\"}";
}

/*
 * React UI API: Cameras list
 * Returns list of configured cameras
 */
void cls_webu_json::api_cameras()
{
    int indx_cam;
    std::string strid;
    cls_camera *cam;

    webua->resp_page = "{\"cameras\":[";

    for (indx_cam=0; indx_cam<app->cam_cnt; indx_cam++) {
        cam = app->cam_list[indx_cam];
        strid = std::to_string(cam->cfg->device_id);

        if (indx_cam > 0) {
            webua->resp_page += ",";
        }

        webua->resp_page += "{";
        webua->resp_page += "\"id\":" + strid + ",";

        if (cam->cfg->device_name == "") {
            webua->resp_page += "\"name\":\"camera " + strid + "\",";
        } else {
            webua->resp_page += "\"name\":\"" + escstr(cam->cfg->device_name) + "\",";
        }

        webua->resp_page += "\"url\":\"" + webua->hostfull + "/" + strid + "/\"";
        webua->resp_page += "}";
    }

    webua->resp_page += "]}";
    webua->resp_type = WEBUI_RESP_JSON;
}

/*
 * React UI API: Configuration
 * Returns full Motion configuration including parameters and categories
 * Includes CSRF token for React UI authentication
 */
void cls_webu_json::api_config()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Add CSRF token at the start of the response */
    webua->resp_page = "{\"csrf_token\":\"" + webu->csrf_token + "\"";

    /* Add version - config() normally starts with { so we skip it */
    webua->resp_page += ",\"version\" : \"" VERSION "\"";

    /* Add cameras list */
    webua->resp_page += ",\"cameras\" : ";
    cameras_list();

    /* Add configuration parameters */
    webua->resp_page += ",\"configuration\" : ";
    parms_all();

    /* Add categories */
    webua->resp_page += ",\"categories\" : ";
    categories_list();

    webua->resp_page += "}";
}

/*
 * React UI API: Batch Configuration Update
 * PATCH /0/api/config with JSON body containing multiple parameters
 * Returns detailed results for each parameter change
 */
void cls_webu_json::api_config_patch()
{
    webua->resp_type = WEBUI_RESP_JSON;

    /* Validate CSRF token */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (csrf_token == nullptr || !webu->csrf_validate(std::string(csrf_token))) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("CSRF token validation failed for PATCH from %s"), webua->clientip.c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"CSRF validation failed\"}";
        return;
    }

    /* Parse JSON body */
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
            _("JSON parse error: %s"), parser.getError().c_str());
        webua->resp_page = "{\"status\":\"error\",\"message\":\"Invalid JSON: " +
                          parser.getError() + "\"}";
        return;
    }

    /* Get config for this camera/device */
    cls_config *cfg;
    if (webua->cam != nullptr) {
        cfg = webua->cam->cfg;
    } else {
        cfg = app->cfg;
    }

    /* Start response */
    webua->resp_page = "{\"status\":\"ok\",\"applied\":[";
    bool first_item = true;
    int success_count = 0;
    int error_count = 0;

    /* Process each parameter */
    pthread_mutex_lock(&app->mutex_post);
    for (const auto& kv : parser.getAll()) {
        std::string parm_name = kv.first;
        std::string parm_val = parser.getString(parm_name);
        std::string old_val;
        int parm_index = -1;
        bool applied = false;
        bool hot_reload = false;
        bool unchanged = false;
        std::string error_msg;

        /* SECURITY: Reject SQL parameter modifications */
        if (parm_name.substr(0, 4) == "sql_") {
            error_msg = "SQL parameters cannot be modified via web interface (security restriction)";
            error_count++;
        }
        /* Validate parameter exists and check if hot-reloadable */
        else if (!validate_hot_reload(parm_name, parm_index)) {
            if (parm_index >= 0) {
                error_msg = "Parameter requires daemon restart";
            } else {
                error_msg = "Unknown parameter";
            }
            error_count++;
        }
        /* Apply the change */
        else {
            cfg->edit_get(parm_name, old_val, config_parms[parm_index].parm_cat);

            /* Check if value actually changed */
            if (old_val == parm_val) {
                unchanged = true;
            } else {
                apply_hot_reload(parm_index, parm_val);
                applied = true;
            }
            hot_reload = true;
            success_count++;
        }

        /* Add this parameter to response */
        if (!first_item) {
            webua->resp_page += ",";
        }
        first_item = false;

        webua->resp_page += "{\"param\":\"" + parm_name + "\"";
        webua->resp_page += ",\"old\":\"" + escstr(old_val) + "\"";
        webua->resp_page += ",\"new\":\"" + escstr(parm_val) + "\"";

        if (unchanged) {
            webua->resp_page += ",\"unchanged\":true";
        } else if (applied) {
            webua->resp_page += ",\"hot_reload\":" + std::string(hot_reload ? "true" : "false");
        }

        if (!error_msg.empty()) {
            webua->resp_page += ",\"error\":\"" + escstr(error_msg) + "\"";
        }

        webua->resp_page += "}";
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page += "]";
    webua->resp_page += ",\"summary\":{";
    webua->resp_page += "\"total\":" + std::to_string(success_count + error_count);
    webua->resp_page += ",\"success\":" + std::to_string(success_count);
    webua->resp_page += ",\"errors\":" + std::to_string(error_count);
    webua->resp_page += "}}";
}

/*
 * React UI API: Get mask information
 * GET /{camId}/api/mask/{type}
 * type = "motion" or "privacy"
 */
void cls_webu_json::api_mask_get()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Get current mask path from config */
    std::string mask_path;
    if (type == "motion") {
        mask_path = webua->cam->cfg->mask_file;
    } else {
        mask_path = webua->cam->cfg->mask_privacy;
    }

    webua->resp_page = "{";
    webua->resp_page += "\"type\":\"" + type + "\"";

    if (mask_path.empty()) {
        webua->resp_page += ",\"exists\":false";
        webua->resp_page += ",\"path\":\"\"";
    } else {
        /* Check if file exists and get dimensions */
        FILE *f = myfopen(mask_path.c_str(), "rbe");
        if (f != nullptr) {
            char line[256];
            int w = 0, h = 0;

            /* Skip magic number P5 */
            if (fgets(line, sizeof(line), f)) {
                /* Skip comments */
                do {
                    if (!fgets(line, sizeof(line), f)) break;
                } while (line[0] == '#');

                /* Parse dimensions */
                sscanf(line, "%d %d", &w, &h);
            }
            myfclose(f);

            webua->resp_page += ",\"exists\":true";
            webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
            webua->resp_page += ",\"width\":" + std::to_string(w);
            webua->resp_page += ",\"height\":" + std::to_string(h);
        } else {
            webua->resp_page += ",\"exists\":false";
            webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
            webua->resp_page += ",\"error\":\"File not accessible\"";
        }
    }

    webua->resp_page += "}";
}

/*
 * React UI API: Save mask from polygon data
 * POST /{camId}/api/mask/{type}
 * Request body: {"polygons":[[{x,y},...]], "width":W, "height":H, "invert":bool}
 */
void cls_webu_json::api_mask_post()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Validate CSRF */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (csrf_token == nullptr || !webu->csrf_validate(std::string(csrf_token))) {
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Parse JSON request body */
    std::string body = webua->raw_body;

    /* Extract dimensions - default to camera size */
    int img_width = webua->cam->imgs.width;
    int img_height = webua->cam->imgs.height;
    bool invert = false;

    /* Parse width/height from body if present */
    size_t pos = body.find("\"width\":");
    if (pos != std::string::npos) {
        img_width = atoi(body.c_str() + pos + 8);
    }
    pos = body.find("\"height\":");
    if (pos != std::string::npos) {
        img_height = atoi(body.c_str() + pos + 9);
    }
    pos = body.find("\"invert\":");
    if (pos != std::string::npos) {
        invert = (body.substr(pos + 9, 4) == "true");
    }

    /* Validate dimensions match camera */
    if (img_width != webua->cam->imgs.width || img_height != webua->cam->imgs.height) {
        MOTION_LOG(WRN, TYPE_ALL, NO_ERRNO,
            "Mask dimensions %dx%d differ from camera %dx%d, will be resized on load",
            img_width, img_height, webua->cam->imgs.width, webua->cam->imgs.height);
    }

    /* Allocate bitmap */
    u_char default_val = invert ? 255 : 0;  /* 255=detect, 0=mask */
    u_char fill_val = invert ? 0 : 255;
    std::vector<u_char> bitmap(img_width * img_height, default_val);

    /* Parse polygons array */
    /* Format: "polygons":[[[x,y],[x,y],...],[[x,y],...]] */
    pos = body.find("\"polygons\":");
    if (pos != std::string::npos) {
        size_t start = body.find('[', pos);
        if (start != std::string::npos) {
            start++; /* Skip outer [ */

            while (start < body.length() && body[start] != ']') {
                /* Skip whitespace */
                while (start < body.length() &&
                       (body[start] == ' ' || body[start] == '\n' || body[start] == ',')) {
                    start++;
                }

                if (body[start] == '[') {
                    /* Parse one polygon */
                    std::vector<std::pair<int,int>> polygon;
                    start++; /* Skip [ */

                    while (start < body.length() && body[start] != ']') {
                        /* Skip to { or [ */
                        while (start < body.length() &&
                               body[start] != '{' && body[start] != '[' && body[start] != ']') {
                            start++;
                        }
                        if (body[start] == ']') break;

                        /* Parse point {x:N, y:N} or [x,y] */
                        int x = 0, y = 0;
                        if (body[start] == '{') {
                            /* Object format */
                            size_t xpos = body.find("\"x\":", start);
                            size_t ypos = body.find("\"y\":", start);
                            if (xpos != std::string::npos && ypos != std::string::npos) {
                                x = atoi(body.c_str() + xpos + 4);
                                y = atoi(body.c_str() + ypos + 4);
                            }
                            start = body.find('}', start) + 1;
                        } else if (body[start] == '[') {
                            /* Array format [x,y] */
                            start++;
                            x = atoi(body.c_str() + start);
                            size_t comma = body.find(',', start);
                            if (comma != std::string::npos) {
                                y = atoi(body.c_str() + comma + 1);
                            }
                            start = body.find(']', start) + 1;
                        }

                        polygon.push_back({x, y});
                    }
                    start++; /* Skip ] */

                    /* Fill polygon */
                    if (polygon.size() >= 3) {
                        fill_polygon(bitmap.data(), img_width, img_height, polygon, fill_val);
                    }
                } else {
                    break;
                }
            }
        }
    }

    /* Generate mask path */
    std::string mask_path = build_mask_path(webua->cam, type);

    /* Write PGM file */
    FILE *f = myfopen(mask_path.c_str(), "wbe");
    if (f == nullptr) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
            "Cannot write mask file: %s", mask_path.c_str());
        webua->resp_page = "{\"error\":\"Cannot write mask file\"}";
        return;
    }

    /* Write PGM P5 header */
    fprintf(f, "P5\n");
    fprintf(f, "# Motion mask - type: %s\n", type.c_str());
    fprintf(f, "%d %d\n", img_width, img_height);
    fprintf(f, "255\n");

    /* Write bitmap data */
    if (fwrite(bitmap.data(), 1, bitmap.size(), f) != bitmap.size()) {
        MOTION_LOG(ERR, TYPE_ALL, SHOW_ERRNO,
            "Failed writing mask data to: %s", mask_path.c_str());
        myfclose(f);
        webua->resp_page = "{\"error\":\"Failed writing mask data\"}";
        return;
    }

    myfclose(f);

    /* Update config parameter */
    pthread_mutex_lock(&app->mutex_post);
    if (type == "motion") {
        webua->cam->cfg->mask_file = mask_path;
        app->cfg->edit_set("mask_file", mask_path);
    } else {
        webua->cam->cfg->mask_privacy = mask_path;
        app->cfg->edit_set("mask_privacy", mask_path);
    }
    pthread_mutex_unlock(&app->mutex_post);

    MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
        "Mask saved: %s (type=%s, %dx%d, polygons parsed)",
        mask_path.c_str(), type.c_str(), img_width, img_height);

    webua->resp_page = "{";
    webua->resp_page += "\"success\":true";
    webua->resp_page += ",\"path\":\"" + escstr(mask_path) + "\"";
    webua->resp_page += ",\"width\":" + std::to_string(img_width);
    webua->resp_page += ",\"height\":" + std::to_string(img_height);
    webua->resp_page += ",\"message\":\"Mask saved. Reload camera to apply.\"";
    webua->resp_page += "}";
}

/*
 * React UI API: Delete mask file
 * DELETE /{camId}/api/mask/{type}
 */
void cls_webu_json::api_mask_delete()
{
    webua->resp_type = WEBUI_RESP_JSON;

    if (webua->cam == nullptr) {
        webua->resp_page = "{\"error\":\"Camera not specified\"}";
        return;
    }

    std::string type = webua->uri_cmd3;
    if (type != "motion" && type != "privacy") {
        webua->resp_page = "{\"error\":\"Invalid mask type. Use 'motion' or 'privacy'\"}";
        return;
    }

    /* Validate CSRF */
    const char* csrf_token = MHD_lookup_connection_value(
        webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
    if (csrf_token == nullptr || !webu->csrf_validate(std::string(csrf_token))) {
        webua->resp_page = "{\"error\":\"CSRF validation failed\"}";
        return;
    }

    /* Get current mask path */
    std::string mask_path;
    if (type == "motion") {
        mask_path = webua->cam->cfg->mask_file;
    } else {
        mask_path = webua->cam->cfg->mask_privacy;
    }

    bool file_deleted = false;
    if (!mask_path.empty()) {
        /* Security: Validate path doesn't contain traversal */
        if (mask_path.find("..") != std::string::npos) {
            MOTION_LOG(ERR, TYPE_STREAM, NO_ERRNO,
                "Path traversal attempt blocked: %s", mask_path.c_str());
            webua->resp_page = "{\"error\":\"Invalid path\"}";
            return;
        }

        /* Delete file */
        if (remove(mask_path.c_str()) == 0) {
            file_deleted = true;
            MOTION_LOG(INF, TYPE_ALL, NO_ERRNO,
                "Deleted mask file: %s", mask_path.c_str());
        } else if (errno != ENOENT) {
            MOTION_LOG(WRN, TYPE_ALL, SHOW_ERRNO,
                "Failed to delete mask file: %s", mask_path.c_str());
        }
    }

    /* Clear config parameter */
    pthread_mutex_lock(&app->mutex_post);
    if (type == "motion") {
        webua->cam->cfg->mask_file = "";
        app->cfg->edit_set("mask_file", "");
    } else {
        webua->cam->cfg->mask_privacy = "";
        app->cfg->edit_set("mask_privacy", "");
    }
    pthread_mutex_unlock(&app->mutex_post);

    webua->resp_page = "{";
    webua->resp_page += "\"success\":true";
    webua->resp_page += ",\"deleted\":" + std::string(file_deleted ? "true" : "false");
    webua->resp_page += ",\"message\":\"Mask removed. Reload camera to apply.\"";
    webua->resp_page += "}";
}

void cls_webu_json::main()
{
    pthread_mutex_lock(&app->mutex_post);
        if (webua->uri_cmd1 == "config.json") {
            config();
        } else if (webua->uri_cmd1 == "movies.json") {
            movies();
        } else if (webua->uri_cmd1 == "status.json") {
            status();
        } else if (webua->uri_cmd1 == "log") {
            loghistory();
        } else {
            webua->bad_request();
            pthread_mutex_unlock(&app->mutex_post);
            return;
        }
    pthread_mutex_unlock(&app->mutex_post);
    webua->mhd_send();
}

cls_webu_json::cls_webu_json(cls_webu_ans *p_webua)
{
    app    = p_webua->app;
    webu   = p_webua->webu;
    webua  = p_webua;
}

cls_webu_json::~cls_webu_json()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
}