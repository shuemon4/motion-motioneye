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
#include "picture.hpp"
#include "webu.hpp"
#include "webu_ans.hpp"
#include "webu_file.hpp"
#include "dbse.hpp"

#include <climits>  /* For PATH_MAX */

/**
 * Validate that a requested file path is within the allowed base directory
 * Prevents path traversal attacks (e.g., ../../../etc/passwd)
 *
 * Uses realpath() to resolve symlinks and relative components,
 * then verifies the resolved path starts with the allowed base.
 *
 * @param requested_path The full path requested (may contain ../ or symlinks)
 * @param allowed_base The base directory that files must be within
 * @return true if the path is safe, false if it's outside allowed_base
 */
static bool validate_file_path(const std::string &requested_path,
    const std::string &allowed_base)
{
    char resolved_request[PATH_MAX];
    char resolved_base[PATH_MAX];

    /* Resolve the requested path to its canonical form */
    if (realpath(requested_path.c_str(), resolved_request) == nullptr) {
        /* File doesn't exist or path is invalid - this is not necessarily
         * a traversal attack, but we can't verify it's safe */
        return false;
    }

    /* Resolve the allowed base directory */
    if (realpath(allowed_base.c_str(), resolved_base) == nullptr) {
        /* Base directory doesn't exist - configuration error */
        return false;
    }

    std::string req_str(resolved_request);
    std::string base_str(resolved_base);

    /* Ensure base path ends with / for proper prefix matching */
    if (base_str.back() != '/') {
        base_str += '/';
    }

    /* Check that resolved request starts with allowed base
     * This prevents:
     * - ../../../etc/passwd -> /etc/passwd (doesn't start with /home/motion/videos/)
     * - Symlink escapes: /videos/link -> /etc (resolved path is /etc/...)
     */
    if (req_str.length() < base_str.length()) {
        return false;
    }

    return (req_str.compare(0, base_str.length(), base_str) == 0);
}

/* Callback for the file reader response*/
static ssize_t webu_file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
    cls_webu_ans *webu_ans =(cls_webu_ans *)cls;
    (void)fseek (webu_ans->req_file, (long)pos, SEEK_SET);
    return (ssize_t)fread (buf, 1, max, webu_ans->req_file);
}

void cls_webu_file::main() {
    mhdrslt retcd;
    struct stat statbuf;
    struct MHD_Response *response;
    std::string full_nm;
    vec_files flst;
    int indx;
    std::string sql;

    /*If we have not fully started yet, simply return*/
    if (app->dbse == NULL) {
        webua->bad_request();
        return;
    }

    for (indx=0;indx<webu->wb_actions->params_cnt;indx++) {
        if (webu->wb_actions->params_array[indx].param_name == "movies") {
            if (webu->wb_actions->params_array[indx].param_value == "off") {
                MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Movies via webcontrol disabled");
                webua->bad_request();
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
    if (flst.size() == 0) {
        webua->bad_request();
        return;
    }

    full_nm = "";
    for (indx=0;indx<(int)flst.size();indx++) {
        if (flst[indx].file_nm == webua->uri_cmd2) {
            full_nm = flst[indx].full_nm;
        }
    }

    /* SECURITY: Validate path before serving file to prevent path traversal attacks
     * This catches:
     * - Database entries modified to contain ../../../etc/passwd
     * - Symlink escapes from target_dir
     * - URL-encoded traversal attempts (already decoded by this point)
     */
    if (!full_nm.empty() && !validate_file_path(full_nm, webua->cam->cfg->target_dir)) {
        MOTION_LOG(ALR, TYPE_STREAM, NO_ERRNO,
            _("Path traversal attempt blocked: %s requested %s (resolved outside %s) from %s"),
            webua->uri_cmd2.c_str(), full_nm.c_str(),
            webua->cam->cfg->target_dir.c_str(), webua->clientip.c_str());
        webua->bad_request();
        return;
    }

    if (stat(full_nm.c_str(), &statbuf) == 0) {
        webua->req_file = myfopen(full_nm.c_str(), "rbe");
    } else {
        webua->req_file = nullptr;
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            ,"Security warning: Client IP %s requested file: %s"
            ,webua->clientip.c_str(), webua->uri_cmd2.c_str());
    }

    if (webua->req_file == nullptr) {
        webua->resp_page = "<html><head><title>Bad File</title>"
            "</head><body>Bad File</body></html>";
        webua->resp_type = WEBUI_RESP_HTML;
        webua->mhd_send();
        retcd = MHD_YES;
    } else {
        response = MHD_create_response_from_callback (
            (size_t)statbuf.st_size, 32 * 1024
            , &webu_file_reader
            , webua, NULL);
        if (response == NULL) {
            if (webua->req_file != nullptr) {
                myfclose(webua->req_file);
                webua->req_file = nullptr;
            }
            webua->bad_request();
            return;
        }
        retcd = MHD_queue_response (webua->connection, MHD_HTTP_OK, response);
        MHD_destroy_response (response);
    }
    if (retcd == MHD_NO) {
        MOTION_LOG(INF, TYPE_ALL, NO_ERRNO, "Error processing file request");
    }

}

cls_webu_file::cls_webu_file(cls_webu_ans *p_webua)
{
    app     = p_webua->app;
    webu    = p_webua->webu;
    webua   = p_webua;
}

cls_webu_file::~cls_webu_file()
{
    app    = nullptr;
    webu   = nullptr;
    webua  = nullptr;
}