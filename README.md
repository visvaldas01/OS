# OS

## Сборка и загрузка

```bash
# для уровня ядра
cd kernel/
make
# загрузка модуля в ядро
sudo insmod labmod.ko
# для уровня пользователя
cd ../user/
cc user.c
```

## Запуск

```bash
# address_space
sudo ../user/a.out --filename=a.out
# vm_area_struct
sudo ../user/a.out --pid=$$
```