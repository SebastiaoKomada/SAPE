const TZ = 'America/Sao_Paulo'

const dateTime = new Intl.DateTimeFormat('pt-BR', {
  timeZone: TZ,
  day: '2-digit',
  month: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
})
const time = new Intl.DateTimeFormat('pt-BR', { timeZone: TZ, hour: '2-digit', minute: '2-digit' })
const dayTime = new Intl.DateTimeFormat('pt-BR', {
  timeZone: TZ,
  day: '2-digit',
  month: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
})
const day = new Intl.DateTimeFormat('pt-BR', { timeZone: TZ, day: '2-digit', month: '2-digit' })

export const formatDateTime = (iso: string) => dateTime.format(new Date(iso))
export const formatTime = (iso: string) => time.format(new Date(iso))
export const formatDayTime = (iso: string) => dayTime.format(new Date(iso))
export const formatDay = (iso: string) => day.format(new Date(iso))

export function formatNumber(value: number | null | undefined, digits = 2, unit = ''): string {
  if (value == null) return '—'
  const n = value.toLocaleString('pt-BR', {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits,
  })
  return unit ? `${n} ${unit}` : n
}

export function formatSigned(value: number | null | undefined, digits = 1, unit = ''): string {
  if (value == null) return '—'
  const sign = value > 0 ? '+' : ''
  return sign + formatNumber(value, digits, unit)
}

export function timeAgo(iso: string | null): string {
  if (!iso) return 'nunca'
  const s = Math.max(0, Math.round((Date.now() - new Date(iso).getTime()) / 1000))
  if (s < 60) return `há ${s} s`
  const m = Math.round(s / 60)
  if (m < 60) return `há ${m} min`
  const h = Math.round(m / 60)
  if (h < 48) return `há ${h} h`
  return `há ${Math.round(h / 24)} dias`
}
