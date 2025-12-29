import { getCsrfToken, setCsrfToken, invalidateCsrfToken } from './csrf';

const API_TIMEOUT = 10000; // 10 seconds
const MAX_RETRIES = 1; // Max retries for transient failures

/**
 * Error code to user-friendly message mapping
 * Based on HTTP status codes and Motion-specific error patterns
 */
const ERROR_MESSAGES: Record<number, string> = {
  400: 'Invalid request. Please check your input.',
  401: 'Authentication required. Please sign in.',
  403: 'Access denied. You do not have permission for this action.',
  404: 'Resource not found.',
  408: 'Request timed out. Please try again.',
  500: 'Server error. Please try again later.',
  502: 'Server unavailable. Motion may be restarting.',
  503: 'Service unavailable. Motion may be restarting.',
  504: 'Gateway timeout. Please try again.',
};

/**
 * Callback type for authentication errors
 * Called when 401/403 is received and auth may be required
 */
type AuthErrorCallback = (status: number) => void;

/** Global auth error callback - set by AuthProvider */
let authErrorCallback: AuthErrorCallback | null = null;

/**
 * Register a callback to be called on authentication errors
 */
export function setAuthErrorCallback(callback: AuthErrorCallback | null): void {
  authErrorCallback = callback;
}

class ApiClientError extends Error {
  status?: number;
  userMessage: string;

  constructor(message: string, status?: number) {
    super(message);
    this.name = 'ApiClientError';
    this.status = status;
    // User-friendly message
    this.userMessage = status ? (ERROR_MESSAGES[status] ?? message) : message;
  }
}

/**
 * Check if error is transient and should be retried
 */
function isTransientError(status: number): boolean {
  return status === 502 || status === 503 || status === 504;
}

/**
 * Sleep for specified milliseconds
 */
function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export async function apiGet<T>(endpoint: string, retryCount = 0): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  try {
    const response = await fetch(endpoint, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
      },
      credentials: 'same-origin',
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    if (!response.ok) {
      // Handle authentication errors
      if (response.status === 401) {
        authErrorCallback?.(401);
        throw new ApiClientError('Authentication required', 401);
      }

      // Handle transient errors with retry
      if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
        await sleep(1000 * (retryCount + 1)); // Exponential backoff
        return apiGet<T>(endpoint, retryCount + 1);
      }

      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    return response.json();
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

export async function apiPost<T>(
  endpoint: string,
  data: Record<string, unknown>,
  retryCount = 0
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const token = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(token && { 'X-CSRF-Token': token }),
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      authErrorCallback?.(401);
      throw new ApiClientError('Authentication required', 401);
    }

    // Handle 403 - token may be stale (Motion restarted)
    if (response.status === 403) {
      invalidateCsrfToken();

      // Refetch config to get new token
      const configResponse = await fetch('/0/api/config');
      if (configResponse.ok) {
        const config = await configResponse.json();
        if (config.csrf_token) {
          setCsrfToken(config.csrf_token);

          // Retry with new token
          const retryResponse = await fetch(endpoint, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              'Accept': 'application/json',
              'X-CSRF-Token': config.csrf_token,
            },
            credentials: 'same-origin',
            body: JSON.stringify(data),
          });

          if (!retryResponse.ok) {
            // If still failing, could be an auth issue
            if (retryResponse.status === 401 || retryResponse.status === 403) {
              authErrorCallback?.(retryResponse.status);
            }
            throw new ApiClientError('CSRF validation failed after retry', 403);
          }

          const text = await retryResponse.text();
          return text ? JSON.parse(text) : ({} as T);
        }
      }
      throw new ApiClientError('CSRF validation failed', 403);
    }

    // Handle transient errors with retry
    if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
      await sleep(1000 * (retryCount + 1)); // Exponential backoff
      return apiPost<T>(endpoint, data, retryCount + 1);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    // Some endpoints return empty response
    const text = await response.text();
    return text ? JSON.parse(text) : ({} as T);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

/**
 * PATCH request for batch configuration updates
 */
export async function apiPatch<T>(
  endpoint: string,
  data: Record<string, unknown>,
  retryCount = 0
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const token = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(token && { 'X-CSRF-Token': token }),
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      authErrorCallback?.(401);
      throw new ApiClientError('Authentication required', 401);
    }

    // Handle 403 - token may be stale (Motion restarted)
    if (response.status === 403) {
      invalidateCsrfToken();

      // Refetch config to get new token
      const configResponse = await fetch('/0/api/config');
      if (configResponse.ok) {
        const config = await configResponse.json();
        if (config.csrf_token) {
          setCsrfToken(config.csrf_token);

          // Retry with new token
          const retryResponse = await fetch(endpoint, {
            method: 'PATCH',
            headers: {
              'Content-Type': 'application/json',
              'Accept': 'application/json',
              'X-CSRF-Token': config.csrf_token,
            },
            credentials: 'same-origin',
            body: JSON.stringify(data),
          });

          if (!retryResponse.ok) {
            // If still failing, could be an auth issue
            if (retryResponse.status === 401 || retryResponse.status === 403) {
              authErrorCallback?.(retryResponse.status);
            }
            throw new ApiClientError('CSRF validation failed after retry', 403);
          }

          const text = await retryResponse.text();
          return text ? JSON.parse(text) : ({} as T);
        }
      }
      throw new ApiClientError('CSRF validation failed', 403);
    }

    // Handle transient errors with retry
    if (isTransientError(response.status) && retryCount < MAX_RETRIES) {
      await sleep(1000 * (retryCount + 1)); // Exponential backoff
      return apiPatch<T>(endpoint, data, retryCount + 1);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    const text = await response.text();
    return text ? JSON.parse(text) : ({} as T);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

/**
 * DELETE request for media file deletion
 */
export async function apiDelete<T>(endpoint: string): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const token = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'DELETE',
      headers: {
        'Accept': 'application/json',
        ...(token && { 'X-CSRF-Token': token }),
      },
      credentials: 'same-origin',
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle authentication errors
    if (response.status === 401) {
      authErrorCallback?.(401);
      throw new ApiClientError('Authentication required', 401);
    }

    // Handle 403 - token may be stale
    if (response.status === 403) {
      invalidateCsrfToken();

      // Refetch config to get new token, then retry
      const configResponse = await fetch('/0/api/config');
      if (configResponse.ok) {
        const config = await configResponse.json();
        if (config.csrf_token) {
          setCsrfToken(config.csrf_token);

          const retryResponse = await fetch(endpoint, {
            method: 'DELETE',
            headers: {
              'Accept': 'application/json',
              'X-CSRF-Token': config.csrf_token,
            },
            credentials: 'same-origin',
          });

          if (!retryResponse.ok) {
            throw new ApiClientError('Delete failed after retry', retryResponse.status);
          }

          const text = await retryResponse.text();
          return text ? JSON.parse(text) : ({} as T);
        }
      }
      throw new ApiClientError('CSRF validation failed', 403);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    const text = await response.text();
    return text ? JSON.parse(text) : ({} as T);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

export { ApiClientError };
