(define 
  (domain worksworld-simplified)
; TODO: Have to test replicate_data replacement (dpa in a out a?).
; TODO: Our makespan calculation remains skewed. We are not tracking the bottleneck steps as the source of the makespan
;        but tracking the sum of all processing components and link latencies used in the resulting graph.

; !LIMITATION: 
; We still cannot detect bottleneck components, which will likely impact the quality of our schedule (we just won't connect to a bottleneck/don't permit them in our model)
; TODO: track if any component has max_msg_rate less than sum of input components max_msg_rate (create replicas or request more cores, if possible, for processing components, change dsi for data components or schedule at multiple sites)
; We have a very simple/abstract representation of workflow component latency (1/max_msg_rate)
; - A more complete representation would be to model component read latency, process latency (being 0 or near zero for data components) and write latency
; - This may be an unnecessary detail if our simple abstraction is good enough

;;;;;;;;;;;;;;
; requirements
;;;;;;;;;;;;;;

  (:requirements 
    :typing 
    :numeric-fluents 
    :equality 
    :negative-preconditions
    :action-costs
    :disjunctive-preconditions 
  )


;;;;;;;;;;;;;;
; types
;;;;;;;;;;;;;;

  (:types
    resource - object ; a finite resource, available or required to execute workflow_components
    
    link - instance ; a logical link over physical network devices between two sites
    composite_link - link ; a logical grouping of two links into a composite link
    direct_link - link ; a logical grouping of two links into a composite link

    interface - instance ; a logical interface for scheduling work
    dsi - interface ; data sharing interface (to schedule data_components)
    dpi - interface ; data processing interface (to schedule processing_components)

    workflow_component - instance ; a node (processing or data sharing) in a user generated workflow
    processing_component - workflow_component ; processing component to schedule on a dpi
    data_component - workflow_component ; data component to schedule on a dsi
    
    descriptor - object ; an object that describes another object
    direction - descriptor ; describes direction of dataflow relative to a processing_component

    workflow_component_type - descriptor ; instances of workflow components may be of the same type
    data_component_type - workflow_component_type ; data_components hold data of a specific type
    processing_component_type - workflow_component_type ; processing components execute code of a specific type
    
    site - object ; a physical location for other objects to reside
  )


;;;;;;;;;;;;;;
; constants
;;;;;;;;;;;;;;

  (:constants
    input output - direction
    config storage compute network - resource
  )


;;;;;;;;;;;;;;
; predicates
;;;;;;;;;;;;;;

  (:predicates
    ; Resource Availability:
    ;;;;;;;;;;;;;;;;;;;;;;;;
    ; A DPI/DSI (with storage or compute) or a workflow component (with code as a container or config) is available at a site:
    (available_at ?inst - instance ?r - resource ?s - site) ; An instance (interface or workflow component) `?inst` with resource `?r` is available at site `?s`
    
    ; Workflow Components:
    ;;;;;;;;;;;;;;;;;;;;;;
    (scheduled_on ?wc - workflow_component ?if - interface) ; a workflow component `?wc` is scheduled on an interface `?if`
    (type_of ?wc - workflow_component ?wct - workflow_component_type) ; workflow component `?wc` is of type `?wct`
    ; Component can or cannot be moved from current site (i.e. sensor fed or consumer present)
    (fixed ?wc - workflow_component) ;`?wc` fixed at current location (set by available_at)
    

    ; Inputs/Outputs:
    ;;;;;;;;;;;;;;;;;
    ; has_input ensures we connect components only when the source has data
    (has_input ?wc - workflow_component ?if - interface) ; a workflow component `?wc` at an interface `?if` has input data
    ; data formats
    (input_format ?pc - processing_component ?dct - data_component_type)  ;processing component ?pc can process data of type ?dct as input.
    (output_format ?pc - processing_component ?dct - data_component_type) ;processing component ?pc generates data of type ?dct as output.
      
    ; Links:
    ;;;;;;;;
    (linked ?lnk - link ?s1 - site ?s2 - site); a composite or direct link `?lnk` connects sites `?s1` and `?s2`
    ; (composite_link ?lnk - link) ; link `?lnk` is a composite link between two sites (NOT direct)
    (link_uses ?lnk_composite - composite_link ?lnk - direct_link); composite link '?lnk_composite' uses direct_link `?lnk`.
    (connected ?lnk - link ?pc - processing_component ?dpi - dpi ?dc - data_component ?dsi - dsi ?dir - direction) ; link `?lnk` connects `?pc` on `?dpi` and `?dc` on `?dsi` with direction `?dir` w.r.t. the `?pc`

    ; Goal Related:
    ;;;;;;;;;;;;;;;
    (processed_by ?dc - data_component ?dsi - dsi ?pc - processing_component ?dpi - dpi) ; data_component `?dc` from interface `?dsi` was processed by component `?pc` on `?dpi`
    (has_data ?dc - data_component ?dsi - dsi ?src_dc - data_component ?src_dsi - dsi); data component `?wc` on interface `?dsi` has data from data component `?src_dc` on interface `?src_dsi`
  ) 


;;;;;;;;;;;;;;
; functions
;;;;;;;;;;;;;;

  (:functions
    ; Resource measures (available and required)
    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    (resource_total ?inst - instance ?rt - resource) ; An instance has a resource amount total (MB for storage, abstract units for compute)
    (resource_available ?inst - instance ?rt - resource) ; An instance has a resource amount available for scheduling
    (work_amount ?wc - workflow_component ?rt - resource) ; A workflow component requires a resource amount to run (MB for storage/config, abstract for compute)
    
    ; Network resource available (bandwidth in MB/s, latency in seconds)
    (network_latency ?lnk - link) ; (s) amount of time one bit takes to traverse a link
    
    ; Workflow Components
    ;;;;;;;;;;;;;;;;;;;;;
    ; All Workflow Components
    (msg_max_rate ?wc - workflow_component) ; (msg/s) maximum rate of messages flowing through a wc
    (msg_actual_rate ?wc - workflow_component ?it - interface) ; (msg/s) actual rate of messages flowing through a wc (set by sensor input or pc output)
    ; Data Components
    (msg_size ?dct - data_component_type) ; (MB) a measure (e.g. avg, median, 95%) of the size of messages passing through a data component
        
    ; Metrics for optimizing/tracking
    (work-cost-weight ?rt - resource) ; how much to weight storage|compute|network cost (1x, 2x, etc)
    (total-cost) ; network, compute and storage cost aggregation (to optimize)
    (total-cost-max) ; place an upper bound on total-cost

    (workflow-makespan) ; (s) single message flowing through components
    (workflow-makespan-max) ; (s) place an upper bound on workflow-makespan
  )


;;;;;;;;;;;;;;;
; replicate_code
;;;;;;;;;;;;;;;

  (:action replicate_code
    :parameters (
      ?wc - workflow_component
      ?from - site
      ?to - site
      ?lnk - link
    )
    :precondition (and 
      (not (= ?from ?to)) 
      (available_at ?wc config ?from) 
      (linked ?lnk ?from ?to)
    )
    :effect (and 
      (available_at ?wc config ?to)
      ; Simplified cost calculations using link
      (increase (total-cost) 
        (*
          (work-cost-weight network)
          (/ (work_amount ?wc config) (resource_total ?lnk network)) 
        )
      )
    )
  )


;;;;;;;;;;;;;;;;;;;;
; schedule_component
;;;;;;;;;;;;;;;;;;;

  (:action schedule_component
    :parameters (
      ?wc - workflow_component
      ?rtwc - resource
      ?it - interface
      ?rtit - resource
      ?its - site
    )
    :precondition (and
      (not (fixed ?wc))
      (not (scheduled_on ?wc ?it))
      (available_at ?wc config ?its)
      (available_at ?it ?rtit ?its)
      (>= (resource_available ?it ?rtit) (work_amount ?wc ?rtwc))
      (= ?rtwc ?rtit)
    )
    :effect (and
      (scheduled_on ?wc ?it)
      (assign (msg_actual_rate ?wc ?it) 0)

      ; Linear total-cost
      (increase
        (total-cost)
        (*
          (work-cost-weight ?rtwc)
          (/ (work_amount ?wc ?rtwc) (resource_total ?it ?rtwc))
        )
      )

      ; Linear workflow-makespan
      (increase
        (workflow-makespan)
        (/ 1 (msg_max_rate ?wc))
      )

      ; Decrease the amount of resources available on the interface:
      (decrease
        (resource_available ?it ?rtit)
        (work_amount ?wc ?rtwc)
      )
    )
  )


;;;;;;;;;;;;;;;;;;;;;
; connect_direct_link
;;;;;;;;;;;;;;;;;;;;;

  (:action connect_direct_link
    :parameters (
      ?pc - processing_component
      ?dpi - dpi
      ?dpis - site
      ?dc - data_component
      ?dct - data_component_type
      ?dsi - dsi
      ?dsis - site
      ?rsv_lnk - direct_link
      ?dir - direction
    )
    :precondition (and
      (not (connected ?rsv_lnk ?pc ?dpi ?dc ?dsi ?dir)) ; don't reserve twice
      (linked ?rsv_lnk ?dpis ?dsis) ; linked in both direction so order irrelevant
      (scheduled_on ?pc ?dpi)
      (scheduled_on ?dc ?dsi)
      (available_at ?dpi compute ?dpis)
      (available_at ?dsi storage ?dsis)
      (type_of ?dc ?dct)

      ; INPUT or OUTPUT predicates met (based on ?dir)
      (or
        (and
          ; INPUT: predicates with dc as source
          (= ?dir input)
          (>=
            (resource_available ?rsv_lnk network)
            (* (msg_size ?dct) (msg_actual_rate ?dc ?dsi))
          )
          (>=
            (msg_max_rate ?pc)
            (+ (msg_actual_rate ?pc ?dpi) (msg_actual_rate ?dc ?dsi))
          )
          (> (msg_actual_rate ?dc ?dsi) 0)
          (input_format ?pc ?dct)
        )
        (and
          ; OUTPUT: predicates with pc as source
          (= ?dir output)
          (>=
            (resource_available ?rsv_lnk network)
            (* (msg_size ?dct) (msg_actual_rate ?pc ?dpi))
          )
          (>=
            (msg_max_rate ?dc)
            (+ (msg_actual_rate ?pc ?dpi) (msg_actual_rate ?dc ?dsi))
          )
          (> (msg_actual_rate ?pc ?dpi) 0) ; Check pc has data
          (output_format ?pc ?dct)
        )
      )
    )
    :effect (and
      (connected ?rsv_lnk ?pc ?dpi ?dc ?dsi ?dir)

      ; Simplified cost using msg_max_rate (msg_actual_rate state-dependent)
      (increase
        (total-cost)
        (*
          (work-cost-weight network)
          (/ (* (msg_size ?dct) (msg_max_rate ?dc)) (resource_total ?rsv_lnk network))
        )
      )
      ; Simplified makespan using one msg and total bandwidth (available NOT linear)
      ; msg_size (MB) / bandwidth (MB/s) = seconds
      (increase
        (workflow-makespan)
        (+
          (/ (msg_size ?dct) (resource_total ?rsv_lnk network))
          (network_latency ?rsv_lnk)
        )
      )
      ; Consume bandwidth
      (decrease
        (resource_available ?rsv_lnk network)
        (* (msg_size ?dct) (+ (msg_actual_rate ?dc ?dsi)(msg_actual_rate ?pc ?dpi))) ;one of these msg_actual_rates is zero (we don't know which until ?dir set)
      )
    )
  )


;;;;;;;;;;;;;;;;;;;;;;;;
; connect_composite_link
;;;;;;;;;;;;;;;;;;;;;;;;

  (:action connect_composite_link
    :parameters (
      ?pc - processing_component
      ?dpi - dpi
      ?dpis - site
      ?dc - data_component
      ?dct - data_component_type
      ?dsi - dsi
      ?dsis - site
      ?rsv_lnk - composite_link
      ?sub_lnk1 - direct_link
      ?sub_lnk2 - direct_link
      ?dir - direction
    )
    :precondition (and
      (not (connected ?rsv_lnk ?pc ?dpi ?dc ?dsi ?dir)) ; don't reserve twice
      (linked ?rsv_lnk ?dpis ?dsis) ; linked in both direction so order irrelevant
      (scheduled_on ?pc ?dpi)
      (scheduled_on ?dc ?dsi)
      (available_at ?dpi compute ?dpis)
      (available_at ?dsi storage ?dsis)
      (type_of ?dc ?dct)

        ; INPUT or OUTPUT predicates met (based on ?dir)
      (or
        (and
          ; INPUT: predicates with dc as source
          (= ?dir input)
          (>= (resource_available ?rsv_lnk network) (* (msg_size ?dct)(msg_actual_rate ?dc ?dsi)))
          ; Satisfy these preconditions here, but generate effects in propagate_input (avoid conditional effects) 
          (>= (msg_max_rate ?pc) (+ (msg_actual_rate ?pc ?dpi)(msg_actual_rate ?dc ?dsi)))
          (> (msg_actual_rate ?dc ?dsi) 0) 
          (input_format ?pc ?dct) 
        )
        (and
          ; OUTPUT: predicates with pc as source
          (= ?dir output)
          ; Satisfy these preconditions here, but generate effects in propagate_output (avoid conditional effects) 
          (>= (resource_available ?rsv_lnk network) (* (msg_size ?dct)(msg_actual_rate ?pc ?dpi)))
          (>= (msg_max_rate ?dc) (+ (msg_actual_rate ?pc ?dpi)(msg_actual_rate ?dc ?dsi)))
          (> (msg_actual_rate ?pc ?dpi) 0)
          (output_format ?pc ?dct)
        )
      )
      (link_uses ?rsv_lnk ?sub_lnk1)
      (link_uses ?rsv_lnk ?sub_lnk2)
      (not (= ?sub_lnk1 ?sub_lnk2))
    )       
    :effect (and
      ; connect components to permit propagate_input|output:
      (connected ?rsv_lnk ?pc ?dpi ?dc ?dsi ?dir)

      ; Linear Cost, without resource_available (state-dependent)
      ; Charge for both sub-link bandwidth usage:
      (increase (total-cost)
        (*
          (work-cost-weight network)
          (+ 
            (/ (* (msg_size ?dct) (msg_max_rate ?dc)) (resource_total ?sub_lnk1 network))
            (/ (* (msg_size ?dct) (msg_max_rate ?dc)) (resource_total ?sub_lnk2 network))
          )
        )
      )
      ; Simplified makespan calculation using bottleneck and composite latency (sum of sub_link latency)
      ; msg_size (MB) / bandwidth (MB/s) = seconds
      (increase (workflow-makespan) 
        (+
          (/ (msg_size ?dct) (resource_total ?sub_lnk1 network)) ; time to send single message based on bandwidth
          (network_latency ?rsv_lnk) 
        )
      )

      (decrease (resource_available ?sub_lnk1 network ) (* (msg_size ?dct) (+ (msg_actual_rate ?dc ?dsi)(msg_actual_rate ?pc ?dpi)))) ;! Assumes pipelined workflow linear chain: one of these msg_actual_rates is zero (we don't know which until ?dir set)
      (decrease (resource_available ?sub_lnk2 network ) (* (msg_size ?dct) (+ (msg_actual_rate ?dc ?dsi)(msg_actual_rate ?pc ?dpi)))) ;! ""
      (decrease (resource_available ?rsv_lnk  network ) (* (msg_size ?dct) (+ (msg_actual_rate ?dc ?dsi)(msg_actual_rate ?pc ?dpi)))) ;! ""
    )
  );connect_composite_link


;* NOTE: The propagate_input|output actions take what would require
;* conditional effects in connect_* and make them their own explicit
;* input/output actions

;;;;;;;;;;;;;;;;;
; propagate_input
;;;;;;;;;;;;;;;;;

  (:action propagate_input
    :parameters (
      ?pc - processing_component
      ?dpi - dpi
      ?dc - data_component
      ?dsi - dsi
      ?src_dc - data_component
      ?src_dsi - dsi
    )
    :precondition (and
      (scheduled_on ?pc ?dpi)
      (scheduled_on ?dc ?dsi)
      (has_input ?dc ?dsi)
      ; Ensure our pc can handle the increased load:
      (>=
        (msg_max_rate ?pc)
        (+ (msg_actual_rate ?dc ?dsi) (msg_actual_rate ?pc ?dpi))
      )
      (exists (?lnk - link) ; exists here to reduce grounding time
        (connected ?lnk ?pc ?dpi ?dc ?dsi input)
      )
      (has_data ?dc ?dsi ?src_dc ?src_dsi)
    )
    :effect (and
      ; For ?src_dc fed through current ?dc, record pc has processed ?src_dc
      (processed_by ?src_dc ?src_dsi ?pc ?dpi)

      ; Declare ?pc has input:
      (has_input ?pc ?dpi)

      ; Track actual rate for future connect predicates:
      (increase
        (msg_actual_rate ?pc ?dpi)
        (msg_actual_rate ?dc ?dsi)
      )
    )
  )


;;;;;;;;;;;;;;;;;;
; propagate_output
;;;;;;;;;;;;;;;;;;

  (:action propagate_output
    :parameters (
      ?pc - processing_component
      ?dpi - dpi
      ?dc - data_component
      ?dsi - dsi
      ?src_dc - data_component
      ?src_dsi - dsi
    )
    :precondition (and
      (scheduled_on ?pc ?dpi)
      (scheduled_on ?dc ?dsi)
      (has_input ?pc ?dpi)
      ; Ensure our dc can handle the increased load:
      (>=
        (msg_max_rate ?dc)
        (+ (msg_actual_rate ?dc ?dsi) (msg_actual_rate ?pc ?dpi))
      )
      (exists (?lnk - link) ; exists here to reduce grounding time
        (connected ?lnk ?pc ?dpi ?dc ?dsi output)
      )
      (processed_by ?src_dc ?src_dsi ?pc ?dpi)
    )
    :effect (and
      ; For ?src_dc processed by current ?pc, record current ?dc data from ?src_dc
      (has_data ?dc ?dsi ?src_dc ?src_dsi)

      ; Declare ?dc has input:
      (has_input ?dc ?dsi)

      ; Track actual rate for future connect predicates:
      (increase
        (msg_actual_rate ?dc ?dsi)
        (msg_actual_rate ?pc ?dpi)
      )
    )
  )
)
