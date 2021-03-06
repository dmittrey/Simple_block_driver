# Simple_block_driver

## Минимальные требования:

1. При загрузке модуля должно создаваться блочное устройство. [x]

2. Размер блочного устройства должен быть равен 100 мегабайтам. [x]

3. С устройства можно читать и писать. [x]

4. При выгрузке модуля устройство должно удалиться. [x]

5. Необходимо наличие комментариев и печать важных сообщений на экран (модуль загружен, устройство создано и т.д.). [x]

6. Код должен собираться на системе с установленным Oracle Linux 8.4 [x]

## Дополнительная функциональность:

1. Зарегистрировать драйвер на шине в sysfs. [x]

2. Добавить атрибут драйвера для взаимодействия с пользователем - для получения команд. [x]

3. Добавить возможность создавать устройства по команде от пользователя, чтобы пользователь сам передал в драйвер имя и размер желаемого устройства. [ ]

4. Добавить параметр модуля, выбирающий вариант создания устройств. Если параметр установлен в "0", то разрешено автоматическое создание и запрещено создание от пользователя. Если выставлен в "1" - разрешено только от пользователя. [ ]

5. Зарегистрировать устройство на шине в sysfs. [ ]

6. Добавить возможность пользователю динамически изменять доступ к конкретному устройству. "0" - read+write, "1" - read only. [ ]

7. Дополнительная задача - добавить возможность динамически изменять размер блочного устройства. [ ]

8. Дополнительная задача - добавить возможность подключать жесткий диск (или виртуальный жесткий диск из VirtualBox/QEMU+KVM) вместо создания буфера устройства в оперативной памяти, клонировать поступающие bio и перенаправлять их в подключенный жесткий диск. [ ]