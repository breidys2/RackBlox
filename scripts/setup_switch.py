#TODO Replace with used front ports
qsfps = [1,2,3,4,..]
#TODO Replace with used lanes
lanes = [0,1,2,3]
for qsfp_cage in qspfs:
    for lane in lanes:
        dp = bfrt.port.port_hdl_info.get(CONN_ID=qsfp_cage,CHNL_ID=lane,print_ents=False).data[b'$DEV_PORT']
        #Example for 25G speed per lane
        bfrt.port.port.add(DEV_PORT=dp,SPEED="BF_SPEED_25G", FEC="BF_FEC_TYP_NONE", AUTO_NEGOTIATION="PM_AN_FORCE_DISABLE", PORT_ENABLE=True)

#TODO If you want to add table entries, do so here
#Uncomment the following line and replace anything with <> with the corresponding value
#bfrt.<p4_program_name>.pipe.Switch[Ingress,Egress].<table_name>.add_with_<action_name>(key=x,parameter1=x,..)
