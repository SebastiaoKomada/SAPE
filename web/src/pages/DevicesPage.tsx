import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { PencilIcon, PlusIcon, Trash2Icon } from 'lucide-react'
import { useState } from 'react'
import { toast } from 'sonner'

import { DeviceFormDialog } from '@/components/DeviceFormDialog'
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '@/components/ui/alert-dialog'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Skeleton } from '@/components/ui/skeleton'
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table'
import { api, type Device } from '@/lib/api'
import { formatNumber, timeAgo } from '@/lib/format'

export default function DevicesPage() {
  const queryClient = useQueryClient()
  const devices = useQuery({ queryKey: ['devices'], queryFn: api.devices })
  const [formOpen, setFormOpen] = useState(false)
  const [editing, setEditing] = useState<Device | undefined>()
  const [deleting, setDeleting] = useState<Device | null>(null)

  const remove = useMutation({
    mutationFn: (id: number) => api.deleteDevice(id),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['devices'] })
      toast.success('Dispositivo excluído')
    },
    onError: (error) => toast.error(error.message),
  })

  const openForm = (device?: Device) => {
    setEditing(device)
    setFormOpen(true)
  }

  return (
    <Card>
      <CardHeader className="flex flex-row flex-wrap items-start justify-between gap-3">
        <div>
          <CardTitle>Dispositivos</CardTitle>
          <CardDescription>Estações de monitoramento cadastradas</CardDescription>
        </div>
        <Button onClick={() => openForm()}>
          <PlusIcon /> Novo dispositivo
        </Button>
      </CardHeader>
      <CardContent>
        {devices.isPending ? (
          <Skeleton className="h-40 w-full" />
        ) : devices.isError ? (
          <p className="text-sm text-destructive">{devices.error.message}</p>
        ) : (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Nome</TableHead>
                <TableHead>Identificador</TableHead>
                <TableHead>Localização</TableHead>
                <TableHead className="text-right">Altura do sensor</TableHead>
                <TableHead>Status</TableHead>
                <TableHead>Conexão</TableHead>
                <TableHead className="w-24" />
              </TableRow>
            </TableHeader>
            <TableBody>
              {devices.data.map((d) => (
                <TableRow key={d.id}>
                  <TableCell className="font-medium">{d.name}</TableCell>
                  <TableCell>
                    <code>{d.identifier}</code>
                  </TableCell>
                  <TableCell>{d.location ?? '—'}</TableCell>
                  <TableCell className="text-right tabular-nums">
                    {formatNumber(d.sensor_height_m, 2, 'm')}
                  </TableCell>
                  <TableCell>
                    <Badge variant={d.status === 'active' ? 'secondary' : 'outline'}>
                      {d.status === 'active' ? 'Ativo' : 'Inativo'}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <div className="flex flex-col">
                      <span className={d.online ? 'text-emerald-600' : 'text-muted-foreground'}>
                        {d.online ? 'Online' : 'Offline'}
                      </span>
                      <span className="text-xs text-muted-foreground">{timeAgo(d.last_seen_at)}</span>
                    </div>
                  </TableCell>
                  <TableCell>
                    <div className="flex justify-end gap-1">
                      <Button variant="ghost" size="icon" aria-label="Editar" onClick={() => openForm(d)}>
                        <PencilIcon />
                      </Button>
                      <Button variant="ghost" size="icon" aria-label="Excluir" onClick={() => setDeleting(d)}>
                        <Trash2Icon />
                      </Button>
                    </div>
                  </TableCell>
                </TableRow>
              ))}
              {devices.data.length === 0 && (
                <TableRow>
                  <TableCell colSpan={7} className="h-20 text-center text-muted-foreground">
                    Nenhum dispositivo cadastrado.
                  </TableCell>
                </TableRow>
              )}
            </TableBody>
          </Table>
        )}
      </CardContent>

      <DeviceFormDialog open={formOpen} onOpenChange={setFormOpen} device={editing} />

      <AlertDialog open={deleting != null} onOpenChange={(open) => !open && setDeleting(null)}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Excluir {deleting?.name}?</AlertDialogTitle>
            <AlertDialogDescription>
              Todas as leituras desse dispositivo também serão apagadas. Não dá para desfazer.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancelar</AlertDialogCancel>
            <AlertDialogAction
              variant="destructive"
              onClick={() => deleting && remove.mutate(deleting.id)}
            >
              Excluir
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </Card>
  )
}
