// Camera from Motion's config
export interface Camera {
  id: number;
  name: string;
  url: string;
  all_xpct_st?: number;
  all_xpct_en?: number;
  all_ypct_st?: number;
  all_ypct_en?: number;
}

// Cameras list response from /0/config
export interface CamerasResponse {
  count: number;
  [key: string]: Camera | number;
}

// Full config response from /0/config
export interface MotionConfig {
  version: string;
  cameras: CamerasResponse;
  configuration: {
    [key: string]: unknown;
  };
  categories: {
    [key: string]: {
      name: string;
      display: string;
    };
  };
}

// Single config parameter value
export interface ConfigParam {
  name: string;
  value: string | number | boolean;
  category?: string;
  type?: 'string' | 'number' | 'boolean' | 'list';
}

// Media item (snapshot or movie)
export interface MediaItem {
  id: number;
  filename: string;
  path: string;
  date: string;
  time?: string;
  size: number;
}

// Pictures API response from /{cam}/api/media/pictures
export interface PicturesResponse {
  pictures: MediaItem[];
}

// System temperature response from /0/api/system/temperature
export interface TemperatureResponse {
  celsius: number;
  fahrenheit: number;
}

// Auth status response from /0/api/auth/me
export interface AuthResponse {
  authenticated: boolean;
  auth_method?: string;
}

// API error
export interface ApiError {
  message: string;
  status?: number;
}
