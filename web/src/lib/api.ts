export type DeviceStatus = 'active' | 'inactive'

export interface Measurement {
  id: number
  distance_m: number | null
  river_level_m: number | null
  sensor_ok: boolean
  battery_v: number
  solar_v: number
  measured_at: string
}

export interface Device {
  id: number
  name: string
  identifier: string
  location: string | null
  sensor_height_m: number | null
  status: DeviceStatus
  last_seen_at: string | null
  online: boolean
  created_at: string
  updated_at: string
  latest_measurement: Measurement | null
}

export interface DeviceInput {
  name: string
  identifier: string
  location: string | null
  sensor_height_m: number | null
  status: DeviceStatus
}

export interface HistoryPoint {
  t: string
  level_avg: number | null
  level_min: number | null
  level_max: number | null
  distance_avg: number
  distance_min: number
  distance_max: number
}

export interface History {
  start: string
  end: string
  bucket_s: number | null
  points: HistoryPoint[]
}

export interface Stats {
  window_min: number
  readings_in_window: number
  failed_in_window: number
  current_distance_m: number | null
  current_level_m: number | null
  level_change_m: number | null
  rise_rate_cm_h: number | null
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`/api${path}`, {
    ...init,
    headers: { 'Content-Type': 'application/json', ...init?.headers },
  })
  if (!res.ok) {
    const body = await res.json().catch(() => null)
    throw new Error(errorMessage(body) ?? `Erro ${res.status}`)
  }
  return (res.status === 204 ? undefined : await res.json()) as T
}

function errorMessage(body: unknown): string | null {
  const detail = (body as { detail?: unknown } | null)?.detail
  if (typeof detail === 'string') return detail
  if (Array.isArray(detail) && detail[0]?.msg) return String(detail[0].msg)
  return null
}

export const api = {
  devices: () => request<Device[]>('/devices'),
  createDevice: (body: DeviceInput) =>
    request<Device>('/devices', { method: 'POST', body: JSON.stringify(body) }),
  updateDevice: (id: number, body: Partial<DeviceInput>) =>
    request<Device>(`/devices/${id}`, { method: 'PATCH', body: JSON.stringify(body) }),
  deleteDevice: (id: number) => request<void>(`/devices/${id}`, { method: 'DELETE' }),
  stats: (id: number) => request<Stats>(`/devices/${id}/stats`),
  recent: (id: number, limit = 20) =>
    request<Measurement[]>(`/devices/${id}/measurements/recent?limit=${limit}`),
  history: (id: number, start: Date, end: Date) =>
    request<History>(
      `/devices/${id}/measurements?` +
        new URLSearchParams({ start: start.toISOString(), end: end.toISOString() }),
    ),
}
