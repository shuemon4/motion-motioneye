import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiGet, apiPost } from './client';
import type {
  MotionConfig,
  Camera,
  PicturesResponse,
  TemperatureResponse,
  AuthResponse,
} from './types';

// Query keys for cache management
export const queryKeys = {
  config: (camId?: number) => ['config', camId] as const,
  cameras: ['cameras'] as const,
  pictures: (camId: number) => ['pictures', camId] as const,
  temperature: ['temperature'] as const,
  auth: ['auth'] as const,
};

// Fetch full Motion config (includes cameras list)
export function useMotionConfig() {
  return useQuery({
    queryKey: queryKeys.config(0),
    queryFn: () => apiGet<MotionConfig>('/0/config'),
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

// Fetch system temperature
export function useTemperature() {
  return useQuery({
    queryKey: queryKeys.temperature,
    queryFn: () => apiGet<TemperatureResponse>('/0/api/system/temperature'),
    refetchInterval: 30000, // Refresh every 30s
  });
}

// Fetch auth status
export function useAuth() {
  return useQuery({
    queryKey: queryKeys.auth,
    queryFn: () => apiGet<AuthResponse>('/0/api/auth/me'),
  });
}

// Update config parameter
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
