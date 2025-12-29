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

#ifndef _INCLUDE_WEBU_JSON_HPP_
#define _INCLUDE_WEBU_JSON_HPP_
    class cls_webu_json {
        public:
            cls_webu_json(cls_webu_ans *p_webua);
            ~cls_webu_json();
            void main();

            /* Hot reload API: Set single parameter at runtime */
            void config_set();

            /* React UI API endpoints */
            void api_auth_me();
            void api_media_pictures();
            void api_system_temperature();
            void api_cameras();

        private:
            cls_motapp      *app;
            cls_webu        *webu;
            cls_webu_ans    *webua;
            void parms_item(cls_config *conf, int indx_parm);
            void parms_one(cls_config *conf);
            void parms_all();
            void cameras_list();
            void categories_list();
            void config();
            void movies_list();
            void movies();
            void status_vars(int indx_cam);
            void status();
            void loghistory();
            std::string escstr(std::string invar);
            void parms_item_detail(cls_config *conf, std::string pNm);

            /* Hot reload helpers */
            bool validate_hot_reload(const std::string &parm_name, int &parm_index);
            void apply_hot_reload(int parm_index, const std::string &parm_val);
            void build_response(bool success, const std::string &parm_name,
                               const std::string &old_val, const std::string &new_val,
                               bool hot_reload);
    };

#endif /* _INCLUDE_WEBU_JSON_HPP_ */
