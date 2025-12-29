import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiGet, apiPost, apiDelete, apiPatch } from './client';
import { setCsrfToken } from './csrf';
import type {
  MotionConfig,
  Camera,
  PicturesResponse,
  MoviesResponse,
  TemperatureResponse,
  SystemStatus,
  AuthResponse,
} from './types';

// Query keys for cache management
export const queryKeys = {
  config: (camId?: number) => ['config', camId] as const,
  cameras: ['cameras'] as const,
  pictures: (camId: number) => ['pictures', camId] as const,
  movies: (camId: number) => ['movies', camId] as const,
  temperature: ['temperature'] as const,
  systemStatus: ['systemStatus'] as const,
  auth: ['auth'] as const,
};

// Fetch full Motion config (includes cameras list)
export function useMotionConfig() {
  return useQuery({
    queryKey: queryKeys.config(0),
    queryFn: async () => {
      const config = await apiGet<MotionConfig>('/0/api/config');
      // Store CSRF token from config response
      if (config.csrf_token) {
        setCsrfToken(config.csrf_token);
      }
      return config;
    },
    staleTime: 60000, // Cache for 1 minute
  });
}

// Fetch camera list from Motion's API
export function useCameras() {
  return useQuery({
    queryKey: queryKeys.cameras,
    queryFn: async () => {
      const response = await apiGet<{ cameras: Camera[] }>('/0/api/cameras');
      return response.cameras;
    },
    staleTime: 60000, // Cache for 1 minute
  });
}

// Fetch pictures for a camera
export function usePictures(camId: number) {
  return useQuery({
    queryKey: queryKeys.pictures(camId),
    queryFn: () => apiGet<PicturesResponse>(`/${camId}/api/media/pictures`),
    staleTime: 30000, // Cache for 30 seconds
  });
}

// Fetch movies for a camera
export function useMovies(camId: number) {
  return useQuery({
    queryKey: queryKeys.movies(camId),
    queryFn: () => apiGet<MoviesResponse>(`/${camId}/api/media/movies`),
    staleTime: 30000, // Cache for 30 seconds
  });
}

// Fetch system temperature
export function useTemperature() {
  return useQuery({
    queryKey: queryKeys.temperature,
    queryFn: () => apiGet<TemperatureResponse>('/0/api/system/temperature'),
    refetchInterval: 30000, // Refresh every 30s
  });
}

// Fetch system status (comprehensive)
export function useSystemStatus() {
  return useQuery({
    queryKey: queryKeys.systemStatus,
    queryFn: () => apiGet<SystemStatus>('/0/api/system/status'),
    refetchInterval: 10000, // Refresh every 10s
    staleTime: 5000,
  });
}

// Fetch auth status
export function useAuth() {
  return useQuery({
    queryKey: queryKeys.auth,
    queryFn: () => apiGet<AuthResponse>('/0/api/auth/me'),
  });
}

// Update config parameter (legacy - single parameter)
// Motion's API uses POST /{camId}/config/set?param=value
export function useUpdateConfig() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      camId,
      param,
      value,
    }: {
      camId: number;
      param: string;
      value: string;
    }) => {
      // Motion expects query parameters, not JSON body
      return apiPost(`/${camId}/config/set?${param}=${encodeURIComponent(value)}`, {});
    },
    onSuccess: (_, { camId }) => {
      // Invalidate config cache to refetch fresh data
      queryClient.invalidateQueries({ queryKey: queryKeys.config(camId) });
      queryClient.invalidateQueries({ queryKey: queryKeys.cameras });
    },
  });
}

// Batch update config parameters (new JSON API)
// PATCH /{camId}/api/config with JSON body
export function useBatchUpdateConfig() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      camId,
      changes,
    }: {
      camId: number;
      changes: Record<string, string | number | boolean>;
    }) => {
      return apiPatch(`/${camId}/api/config`, changes);
    },
    onSuccess: (_, { camId }) => {
      // Invalidate config cache to refetch fresh data
      queryClient.invalidateQueries({ queryKey: queryKeys.config(camId) });
      queryClient.invalidateQueries({ queryKey: queryKeys.cameras });
    },
  });
}

// Delete a picture
export function useDeletePicture() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ camId, pictureId }: { camId: number; pictureId: number }) => {
      return apiDelete<{ success: boolean; deleted_id: number }>(
        `/${camId}/api/media/picture/${pictureId}`
      );
    },
    onSuccess: (_, { camId }) => {
      // Invalidate pictures cache to refetch fresh data
      queryClient.invalidateQueries({ queryKey: queryKeys.pictures(camId) });
    },
  });
}

// Delete a movie
export function useDeleteMovie() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ camId, movieId }: { camId: number; movieId: number }) => {
      return apiDelete<{ success: boolean; deleted_id: number }>(
        `/${camId}/api/media/movie/${movieId}`
      );
    },
    onSuccess: (_, { camId }) => {
      // Invalidate movies cache to refetch fresh data
      queryClient.invalidateQueries({ queryKey: queryKeys.movies(camId) });
    },
  });
}
