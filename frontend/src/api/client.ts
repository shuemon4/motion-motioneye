const API_TIMEOUT = 10000; // 10 seconds

class ApiClientError extends Error {
  status?: number;

  constructor(message: string, status?: number) {
    super(message);
    this.name = 'ApiClientError';
    this.status = status;
  }
}

export async function apiGet<T>(endpoint: string): Promise<T> {
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
  data: Record<string, unknown>
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

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

export { ApiClientError };
