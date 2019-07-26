/******************************************************************************
Copyright (C) 2019 by <rat.with.a.compiler@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <lib/caption_stream/CaptionStream.h>
#include "log.c"
#include <utils.h>
#include "storage_utils.h"

#define SAVE_ENTRY_NAME "cloud_closed_caption_rat"

static CaptionStreamSettings default_CaptionStreamSettings() {
    uint download_start_delay_ms = 4000;
    download_start_delay_ms = 40;
    return {
            5000,
            5000,
            180'000,
            50,
            download_start_delay_ms,
            "en-US",
            0
    };
};


static ContinuousCaptionStreamSettings default_ContinuousCaptionStreamSettings() {
    return {
            260,
            275,
            10,
            default_CaptionStreamSettings()
    };
};

static CaptionFormatSettings default_CaptionFormatSettings() {
    return {
            32,
            3,
            false,
            {"niger", "nigger", "fag", "faggot", "chink"}, // default banned words
            true,
            25.0,
    };
};


static CaptionSourceSettings default_CaptionSourceSettings() {
    return {
            "",
            CAPTION_SOURCE_MUTE_TYPE_FROM_OWN_SOURCE,
            ""
    };
}

static CaptionerSettings default_CaptionerSettings() {
    return CaptionerSettings(
            false,
            default_CaptionSourceSettings(),
            default_CaptionFormatSettings(),
            default_ContinuousCaptionStreamSettings()
    );
};


static void enforce_sensible_values(CaptionerSettings &settings) {
    if (settings.format_settings.caption_line_count <= 0 || settings.format_settings.caption_line_count > 4)
        settings.format_settings.caption_line_count = 1;
}

static CaptionerSettings load_obs_CaptionerSettings(obs_data_t *load_data) {
    info_log("obs load event");

    obs_data_t *obj = obs_data_get_obj(load_data, SAVE_ENTRY_NAME);
    auto settings = default_CaptionerSettings();

    printf("\n\n");
    settings.print();
    if (!obj) {
        info_log("first time loading, keeping default settings");
    } else {
        obs_data_set_default_bool(obj, "enabled", settings.enabled);
        obs_data_set_default_bool(obj, "caption_insert_newlines", settings.format_settings.caption_insert_newlines);
        obs_data_set_default_int(obj, "caption_line_count", settings.format_settings.caption_line_count);
        obs_data_set_default_string(obj, "manual_banned_words", "");
        obs_data_set_default_string(obj, "source_name", settings.caption_source_settings.caption_source_name.c_str());
        obs_data_set_default_string(obj, "mute_source_name", "");
        obs_data_set_default_string(obj, "source_caption_when", "");
        obs_data_set_default_string(obj, "source_language", settings.stream_settings.stream_settings.language.c_str());
        obs_data_set_default_int(obj, "profanity_filter", settings.stream_settings.stream_settings.profanity_filter);

        obs_data_set_default_double(obj, "caption_timeout_secs", settings.format_settings.caption_timeout_seconds);
        obs_data_set_default_bool(obj, "caption_timeout_enabled", settings.format_settings.caption_timeout_enabled);


        settings.enabled = obs_data_get_bool(obj, "enabled");
        settings.format_settings.caption_insert_newlines = obs_data_get_bool(obj, "caption_insert_newlines");
        settings.format_settings.caption_line_count = (int) obs_data_get_int(obj, "caption_line_count");

        settings.caption_source_settings.caption_source_name = obs_data_get_string(obj, "source_name");
        settings.caption_source_settings.mute_source_name = obs_data_get_string(obj, "mute_source_name");
        settings.stream_settings.stream_settings.language = obs_data_get_string(obj, "source_language");
        settings.stream_settings.stream_settings.profanity_filter = (int) obs_data_get_int(obj, "profanity_filter");

        settings.format_settings.caption_timeout_enabled = obs_data_get_bool(obj, "caption_timeout_enabled");
        settings.format_settings.caption_timeout_seconds = obs_data_get_double(obj, "caption_timeout_secs");

        string mute_when_str = obs_data_get_string(obj, "source_caption_when");
        settings.caption_source_settings.mute_when = string_to_mute_setting(mute_when_str, CAPTION_SOURCE_MUTE_TYPE_FROM_OWN_SOURCE);

        string banned_words_line = obs_data_get_string(obj, "manual_banned_words");
        settings.format_settings.manual_banned_words = string_to_banned_words(banned_words_line);

        enforce_sensible_values(settings);
    }
    obs_data_release(obj);
    printf("\n\n");
    settings.print();
    return settings;

}

static void save_obs_CaptionerSettings(obs_data_t *save_data, CaptionerSettings &settings) {
    info_log("obs save event");
    obs_data_t *obj = obs_data_create();

    obs_data_set_bool(obj, "enabled", settings.enabled);
    obs_data_set_int(obj, "caption_line_count", settings.format_settings.caption_line_count);
    obs_data_set_bool(obj, "caption_insert_newlines", settings.format_settings.caption_insert_newlines);
//    obs_data_set_bool(obj, "caption_insert_newlines", settings.format_settings.caption_insert_newlines);


    string caption_when = mute_setting_to_string(settings.caption_source_settings.mute_when, "");
    obs_data_set_string(obj, "source_caption_when", caption_when.c_str());

    obs_data_set_string(obj, "source_name", settings.caption_source_settings.caption_source_name.c_str());
    obs_data_set_string(obj, "mute_source_name", settings.caption_source_settings.mute_source_name.c_str());
    obs_data_set_string(obj, "source_language", settings.stream_settings.stream_settings.language.c_str());
    obs_data_set_int(obj, "profanity_filter", settings.stream_settings.stream_settings.profanity_filter);

    obs_data_set_bool(obj, "caption_timeout_enabled", settings.format_settings.caption_timeout_enabled);
    obs_data_set_double(obj, "caption_timeout_secs", settings.format_settings.caption_timeout_seconds);

    string banned_words_line;
    if (!settings.format_settings.manual_banned_words.empty()) {
//        info_log("savingggggg %d", settings.format_settings.manual_banned_words.size());
        words_to_string(settings.format_settings.manual_banned_words, banned_words_line);
    }
    obs_data_set_string(obj, "manual_banned_words", banned_words_line.c_str());

    obs_data_set_obj(save_data, SAVE_ENTRY_NAME, obj);
    obs_data_release(obj);
}

static bool is_stream_live() {
    obs_output_t *output = obs_frontend_get_streaming_output();
    bool live = !!output;
    obs_output_release(output);
    return live;
}


static void setup_combobox_profanity(QComboBox &comboBox) {
    while (comboBox.count())
        comboBox.removeItem(0);

    comboBox.addItem("None", 0);
    comboBox.addItem("Some (Very Unreliable!)", 1);
    comboBox.addItem("Strict (Very Unreliable!)", 2);
}

static void setup_combobox_languages(QComboBox &comboBox) {
    while (comboBox.count())
        comboBox.removeItem(0);

    comboBox.addItem("English (United States)", "en-US");
    comboBox.addItem("Afrikaans (South Africa)", "af-ZA");
//    comboBox.addItem("Amharic (Ethiopia)", "am-ET");
//    comboBox.addItem("Armenian (Armenia)", "hy-AM");
//    comboBox.addItem("Azerbaijani (Azerbaijan)", "az-AZ");
//    comboBox.addItem("Indonesian (Indonesia)", "id-ID");
//    comboBox.addItem("Malay (Malaysia)", "ms-MY");
//    comboBox.addItem("Bengali (Bangladesh)", "bn-BD");
//    comboBox.addItem("Bengali (India)", "bn-IN");
    comboBox.addItem("Catalan (Spain)", "ca-ES");
    comboBox.addItem("Czech (Czech Republic)", "cs-CZ");
    comboBox.addItem("Danish (Denmark)", "da-DK");
    comboBox.addItem("German (Germany)", "de-DE");
    comboBox.addItem("English (Australia)", "en-AU");
    comboBox.addItem("English (Canada)", "en-CA");
    comboBox.addItem("English (Ghana)", "en-GH");
    comboBox.addItem("English (United Kingdom)", "en-GB");
    comboBox.addItem("English (India)", "en-IN");
    comboBox.addItem("English (Ireland)", "en-IE");
    comboBox.addItem("English (Kenya)", "en-KE");
    comboBox.addItem("English (New Zealand)", "en-NZ");
    comboBox.addItem("English (Nigeria)", "en-NG");
    comboBox.addItem("English (Philippines)", "en-PH");
    comboBox.addItem("English (Singapore)", "en-SG");
    comboBox.addItem("English (South Africa)", "en-ZA");
    comboBox.addItem("English (Tanzania)", "en-TZ");
    comboBox.addItem("Spanish (Argentina)", "es-AR");
    comboBox.addItem("Spanish (Bolivia)", "es-BO");
    comboBox.addItem("Spanish (Chile)", "es-CL");
    comboBox.addItem("Spanish (Colombia)", "es-CO");
    comboBox.addItem("Spanish (Costa Rica)", "es-CR");
    comboBox.addItem("Spanish (Ecuador)", "es-EC");
    comboBox.addItem("Spanish (El Salvador)", "es-SV");
    comboBox.addItem("Spanish (Spain)", "es-ES");
    comboBox.addItem("Spanish (United States)", "es-US");
    comboBox.addItem("Spanish (Guatemala)", "es-GT");
    comboBox.addItem("Spanish (Honduras)", "es-HN");
    comboBox.addItem("Spanish (Mexico)", "es-MX");
    comboBox.addItem("Spanish (Nicaragua)", "es-NI");
    comboBox.addItem("Spanish (Panama)", "es-PA");
    comboBox.addItem("Spanish (Paraguay)", "es-PY");
    comboBox.addItem("Spanish (Peru)", "es-PE");
    comboBox.addItem("Spanish (Puerto Rico)", "es-PR");
    comboBox.addItem("Spanish (Dominican Republic)", "es-DO");
    comboBox.addItem("Spanish (Uruguay)", "es-UY");
    comboBox.addItem("Spanish (Venezuela)", "es-VE");
    comboBox.addItem("Basque (Spain)", "eu-ES");
//    comboBox.addItem("Filipino (Philippines)", "fil-PH");
    comboBox.addItem("French (Canada)", "fr-CA");
    comboBox.addItem("French (France)", "fr-FR");
    comboBox.addItem("Galician (Spain)", "gl-ES");
//    comboBox.addItem("Georgian (Georgia)", "ka-GE");
//    comboBox.addItem("Gujarati (India)", "gu-IN");
    comboBox.addItem("Croatian (Croatia)", "hr-HR");
//    comboBox.addItem("Zulu (South Africa)", "zu-ZA");
    comboBox.addItem("Icelandic (Iceland)", "is-IS");
    comboBox.addItem("Italian (Italy)", "it-IT");
//    comboBox.addItem("Javanese (Indonesia)", "jv-ID");
//    comboBox.addItem("Kannada (India)", "kn-IN");
//    comboBox.addItem("Khmer (Cambodia)", "km-KH");
//    comboBox.addItem("Lao (Laos)", "lo-LA");
    comboBox.addItem("Latvian (Latvia)", "lv-LV");
    comboBox.addItem("Lithuanian (Lithuania)", "lt-LT");
    comboBox.addItem("Hungarian (Hungary)", "hu-HU");
//    comboBox.addItem("Malayalam (India)", "ml-IN");
//    comboBox.addItem("Marathi (India)", "mr-IN");
    comboBox.addItem("Dutch (Netherlands)", "nl-NL");
//    comboBox.addItem("Nepali (Nepal)", "ne-NP");
//    comboBox.addItem("Norwegian Bokmål (Norway)", "nb-NO");
    comboBox.addItem("Polish (Poland)", "pl-PL");
    comboBox.addItem("Portuguese (Brazil)", "pt-BR");
    comboBox.addItem("Portuguese (Portugal)", "pt-PT");
    comboBox.addItem("Romanian (Romania)", "ro-RO");
//    comboBox.addItem("Sinhala (Sri Lanka)", "si-LK");
    comboBox.addItem("Slovak (Slovakia)", "sk-SK");
    comboBox.addItem("Slovenian (Slovenia)", "sl-SI");
//    comboBox.addItem("Sundanese (Indonesia)", "su-ID");
//    comboBox.addItem("Swahili (Tanzania)", "sw-TZ");
//    comboBox.addItem("Swahili (Kenya)", "sw-KE");
//    comboBox.addItem("Finnish (Finland)", "fi-FI");
//    comboBox.addItem("Swedish (Sweden)", "sv-SE");
//    comboBox.addItem("Tamil (India)", "ta-IN");
//    comboBox.addItem("Tamil (Singapore)", "ta-SG");
//    comboBox.addItem("Tamil (Sri Lanka)", "ta-LK");
//    comboBox.addItem("Tamil (Malaysia)", "ta-MY");
//    comboBox.addItem("Telugu (India)", "te-IN");
//    comboBox.addItem("Vietnamese (Vietnam)", "vi-VN");
//    comboBox.addItem("Turkish (Turkey)", "tr-TR");
//    comboBox.addItem("Urdu (Pakistan)", "ur-PK");
//    comboBox.addItem("Urdu (India)", "ur-IN");
//    comboBox.addItem("Greek (Greece)", "el-GR");
//    comboBox.addItem("Bulgarian (Bulgaria)", "bg-BG");
//    comboBox.addItem("Russian (Russia)", "ru-RU");
//    comboBox.addItem("Serbian (Serbia)", "sr-RS");
//    comboBox.addItem("Ukrainian (Ukraine)", "uk-UA");
//    comboBox.addItem("Hebrew (Israel)", "he-IL");
//    comboBox.addItem("Arabic (Israel)", "ar-IL");
//    comboBox.addItem("Arabic (Jordan)", "ar-JO");
//    comboBox.addItem("Arabic (United Arab Emirates)", "ar-AE");
//    comboBox.addItem("Arabic (Bahrain)", "ar-BH");
//    comboBox.addItem("Arabic (Algeria)", "ar-DZ");
//    comboBox.addItem("Arabic (Saudi Arabia)", "ar-SA");
//    comboBox.addItem("Arabic (Iraq)", "ar-IQ");
//    comboBox.addItem("Arabic (Kuwait)", "ar-KW");
//    comboBox.addItem("Arabic (Morocco)", "ar-MA");
//    comboBox.addItem("Arabic (Tunisia)", "ar-TN");
//    comboBox.addItem("Arabic (Oman)", "ar-OM");
//    comboBox.addItem("Arabic (State of Palestine)", "ar-PS");
//    comboBox.addItem("Arabic (Qatar)", "ar-QA");
//    comboBox.addItem("Arabic (Lebanon)", "ar-LB");
//    comboBox.addItem("Arabic (Egypt)", "ar-EG");
//    comboBox.addItem("Persian (Iran)", "fa-IR");
//    comboBox.addItem("Hindi (India)", "hi-IN");
//    comboBox.addItem("Thai (Thailand)", "th-TH");
//    comboBox.addItem("Korean (South Korea)", "ko-KR");
//    comboBox.addItem("Chinese, Mandarin (Traditional, Taiwan)", "zh-TW");
//    comboBox.addItem("Chinese, Cantonese (Traditional, Hong Kong)", "yue-Hant-HK");
//    comboBox.addItem("Japanese (Japan)", "ja-JP");
//    comboBox.addItem("Chinese, Mandarin (Simplified, Hong Kong)", "zh-HK");
//    comboBox.addItem("Chinese, Mandarin (Simplified, China)", "zh");
}