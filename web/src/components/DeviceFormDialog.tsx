import { useMutation, useQueryClient } from '@tanstack/react-query'
import { type FormEvent, useState } from 'react'
import { toast } from 'sonner'

import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Switch } from '@/components/ui/switch'
import { api, type Device, type DeviceInput } from '@/lib/api'

interface Props {
  open: boolean
  onOpenChange: (open: boolean) => void
  // Ausente = cadastro de um novo dispositivo
  device?: Device
}

export function DeviceFormDialog({ open, onOpenChange, device }: Props) {
  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent>
        {/* A key remonta o formulário com os valores do dispositivo escolhido */}
        {open && <DeviceForm key={device?.id ?? 'new'} device={device} onDone={() => onOpenChange(false)} />}
      </DialogContent>
    </Dialog>
  )
}

function DeviceForm({ device, onDone }: { device?: Device; onDone: () => void }) {
  const queryClient = useQueryClient()
  const [name, setName] = useState(device?.name ?? '')
  const [identifier, setIdentifier] = useState(device?.identifier ?? '')
  const [location, setLocation] = useState(device?.location ?? '')
  const [height, setHeight] = useState(device?.sensor_height_m?.toString() ?? '')
  const [active, setActive] = useState((device?.status ?? 'active') === 'active')

  const save = useMutation({
    mutationFn: (body: DeviceInput) =>
      device ? api.updateDevice(device.id, body) : api.createDevice(body),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['devices'] })
      toast.success(device ? 'Dispositivo atualizado' : 'Dispositivo cadastrado')
      onDone()
    },
    onError: (error) => toast.error(error.message),
  })

  const submit = (e: FormEvent) => {
    e.preventDefault()
    const parsedHeight = height.trim() === '' ? null : Number(height.replace(',', '.'))
    if (parsedHeight != null && !(parsedHeight > 0)) {
      toast.error('A altura do sensor deve ser um número maior que zero')
      return
    }
    save.mutate({
      name: name.trim(),
      identifier: identifier.trim(),
      location: location.trim() || null,
      sensor_height_m: parsedHeight,
      status: active ? 'active' : 'inactive',
    })
  }

  const heightChanged = device != null && String(device.sensor_height_m ?? '') !== height.trim()

  return (
    <form onSubmit={submit} className="flex flex-col gap-4">
      <DialogHeader>
        <DialogTitle>{device ? 'Editar dispositivo' : 'Novo dispositivo'}</DialogTitle>
        <DialogDescription>
          Só são gravadas leituras de dispositivos cadastrados e ativos.
        </DialogDescription>
      </DialogHeader>

      <div className="grid gap-2">
        <Label htmlFor="name">Nome</Label>
        <Input id="name" required maxLength={100} value={name} onChange={(e) => setName(e.target.value)} />
      </div>

      <div className="grid gap-2">
        <Label htmlFor="identifier">Identificador</Label>
        <Input
          id="identifier"
          required
          maxLength={100}
          pattern="[a-z0-9][a-z0-9_\-]*"
          placeholder="estacao-01"
          value={identifier}
          onChange={(e) => setIdentifier(e.target.value)}
        />
        <p className="text-xs text-muted-foreground">
          Igual ao <code>DEVICE_ID</code> do firmware. Letras minúsculas, números, - e _.
        </p>
      </div>

      <div className="grid gap-2">
        <Label htmlFor="location">Localização</Label>
        <Input
          id="location"
          maxLength={255}
          placeholder="Ex.: Ponte da Av. Brasil"
          value={location}
          onChange={(e) => setLocation(e.target.value)}
        />
      </div>

      <div className="grid gap-2">
        <Label htmlFor="height">Altura do sensor (m)</Label>
        <Input
          id="height"
          inputMode="decimal"
          placeholder="Ex.: 3,50"
          value={height}
          onChange={(e) => setHeight(e.target.value)}
        />
        <p className="text-xs text-muted-foreground">
          Distância do sensor até o zero do rio. Nível = altura − distância medida.
          {heightChanged && ' A nova altura vale só para as próximas leituras.'}
        </p>
      </div>

      <div className="flex items-center justify-between rounded-md border p-3">
        <div>
          <Label htmlFor="active">Ativo</Label>
          <p className="text-xs text-muted-foreground">Inativo descarta as leituras recebidas</p>
        </div>
        <Switch id="active" checked={active} onCheckedChange={setActive} />
      </div>

      <DialogFooter>
        <Button type="button" variant="outline" onClick={onDone}>
          Cancelar
        </Button>
        <Button type="submit" disabled={save.isPending}>
          {save.isPending ? 'Salvando…' : 'Salvar'}
        </Button>
      </DialogFooter>
    </form>
  )
}
